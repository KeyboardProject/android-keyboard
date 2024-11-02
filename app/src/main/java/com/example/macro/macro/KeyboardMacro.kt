package com.example.macro.macro

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.IBinder
import android.util.Log
import com.example.macro.handler.UsbKeyboardHandler
import com.example.macro.handler.UsbKeyboardHandler.Companion
import com.example.macro.keyboard.GattServerService
import java.io.File
import java.io.FileWriter
import java.util.Timer
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.locks.LockSupport
import kotlin.concurrent.scheduleAtFixedRate

data class KeyEvent(val delay: Long, val data: ByteArray)
data class ComplexReplayRequest (
    val tasks: List<ReplayTask>,   // 재생할 작업 목록
    val repeatCount: Int           // 작업 목록 반복 횟수
)

data class ReplayTask(
    val filename: String,          // 녹화 파일 이름
    val delayAfter: Int,           // 파일 재생 후 지연 시간(초)
    val repeatCount: Int           // 파일 반복 횟수
)

class KeyboardMacro private constructor(private val context: Context) {
    private val inputQueue = ConcurrentLinkedQueue<KeyEvent>()
    private var recordingFile: File? = null
    private var isRecording = false
    private val timer = Timer()
    private var startTime: Long = 0
    private var writer: FileWriter? = null
    private var gattServerService: GattServerService? = null
    var stopFlag = true;

    companion object {
        @Volatile
        private var instance: KeyboardMacro? = null
        var TAG = "KeyboardMacro";

        fun getInstance(context: Context): KeyboardMacro {
            return instance ?: synchronized(this) {
                instance ?: KeyboardMacro(context).also { instance = it }
            }
        }
    }

    // 녹화 시작
    fun startRecord(fileName: String) {
        val appDir = context.filesDir
        recordingFile = File(appDir, fileName)
        recordingFile?.let {
            isRecording = true
            startTime = System.nanoTime()
            writer = FileWriter(it, false)

            // 주기적으로 큐를 비우고 파일에 기록
            timer.scheduleAtFixedRate(0, 1000) {  // 1초마다 실행
                if (isRecording) {
                    processInputs()
                } else {
                    cancel()  // 녹화가 중지되면 타이머 취소
                }
            }
        }
    }

    // 녹화를 중지함.
    fun stopRecord() {
        isRecording = false
        writer?.close()
        writer = null
    }

    fun addInput(data: ByteArray) {
        if (isRecording) {
            val currentTime = System.nanoTime()
            val timeDiff = currentTime - startTime // 경과 시간 계산
            inputQueue.add(KeyEvent(timeDiff, data))
        }
    }

    // 큐에서 하나씩 꺼내서 파일에 기록
    fun processInputs() {
        writer?.let { fileWriter ->
            while (inputQueue.isNotEmpty()) {
                val event = inputQueue.poll()
                fileWriter.write("Time diff: ${"%X".format(event.delay)}ns, Data: ${event.data.joinToString(" ") { "%02x".format(it) }}\n")
            }
            fileWriter.flush()  // 데이터를 주기적으로 기록
        }
    }

    fun loadRecordedFile(fileName: String): List<KeyEvent> {
        val appDir = context.filesDir
        val file = File(appDir, fileName)
        val events = mutableListOf<KeyEvent>()

        if (!file.exists()) {
            println("File $fileName does not exist")
            return events
        }

        file.forEachLine { line ->
            // 정규식을 이용해 시간과 데이터를 추출
            val matchResult = Regex("Time diff: ([A-Fa-f0-9]+)ns, Data: ([0-9a-fA-F ]+)").find(line)
            if (matchResult != null) {
                val (delayHex, dataHex) = matchResult.destructured
                val delay = delayHex.toLong(16) // 16진수 시간을 10진수로 변환
                val data = dataHex.split(" ").map { it.toInt(16).toByte() }.toByteArray()
                events.add(KeyEvent(delay, data))
            }
        }
        return events
    }

    fun stopReplay() {
        stopFlag = true
    }

    private fun startReplay(fileName: String) {
        val events = loadRecordedFile(fileName)

        replayRecordFile(events) { event ->

        }
    }

    fun startComplexReplay(request: ComplexReplayRequest) {
        stopFlag = false
        repeat(request.repeatCount) { j ->
            for(task in request.tasks) {
                repeat(task.repeatCount) { i ->
                    startReplay(task.filename)

                    if (stopFlag) {
                        return
                    };

                    Thread.sleep(task.delayAfter.toLong());
                }
            }

            if (stopFlag) {
                return
            };
        }
        stopFlag = true
    }

    fun startReplayDebug(fileName: String, onEvent: (KeyEvent) -> Unit) {
        stopFlag = false
        val events = loadRecordedFile(fileName)

        replayRecordFile(events, onEvent)
        stopFlag = true
    }

    fun replayRecordFile(events: List<KeyEvent>, onEvent: (KeyEvent) -> Unit) {
        // 현재 시간을 기준으로 시작 시간 설정
        val startTime = System.nanoTime()

        for (event in events) {
            // 이벤트 타겟 시간 계산: 시작 시간 + 이벤트의 지연 시간
            val targetTime = startTime + event.delay

            var remainingTime = targetTime - System.nanoTime()

            // 남은 시간이 1ms 이상일 때는 parkNanos를 사용
            while (remainingTime > 1_000_000) {
                LockSupport.parkNanos(remainingTime)
                remainingTime = targetTime - System.nanoTime()
            }

            // 남은 시간이 1ms 이하일 때는 busy-wait로 전환
            while (System.nanoTime() < targetTime) {
                // Busy-wait: 남은 시간이 매우 짧은 경우, System.nanoTime을 계속 확인하여 정확도 향상
            }

            if (stopFlag) {
                gattServerService?.let { service ->
                    val stopData = ByteArray(8) { 0 }
                    service.sendReport(stopData)
                    Log.d(TAG, "Stop signal sent: ${stopData.joinToString(", ")}")
                } ?: Log.e(TAG, "GattServerService is not connected")
                Log.d(TAG, "Macro replay stopped.")
                break
            }

            val service = gattServerService
            if (service != null) {
                // 서비스의 sendReport() 메서드에 keyData 전달
                service.sendReport(event.data);
            } else {
                Log.e(com.example.macro.handler.UsbKeyboardHandler.TAG, "GattServerService is not connected")
            }

            // 키 입력 이벤트 전송을 위한 콜백 호출
            onEvent(event)
        }
    }

    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            val binder = service as? GattServerService.LocalBinder
            if (binder != null) {
                gattServerService = binder.getService()
                Log.d(UsbKeyboardHandler.TAG, "Service connected")
            } else {
                Log.e(UsbKeyboardHandler.TAG, "Service binding failed")
            }
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            gattServerService = null
            Log.d(UsbKeyboardHandler.TAG, "Service disconnected")
        }
    }

    fun getFileList(): List<String> {
        // 앱 전용 디렉터리 내 파일 목록을 가져옴
        val directory = context.filesDir // 또는 context.getExternalFilesDir(null)

        if (directory.exists() && directory.isDirectory) {
            // 디렉터리 내의 파일 목록을 반환
            return directory.listFiles()
                ?.filter { it.isFile } // 파일만 필터링
                ?.map { it.name }      // 파일 이름만 추출
                ?: emptyList()
        }

        // 디렉터리가 없거나 파일이 없는 경우 빈 리스트 반환
        return emptyList()
    }

    fun cleanup() {
        try {
            context.unbindService(serviceConnection)
            Log.d(TAG, "ServiceConnection unbound")
        } catch (e: IllegalArgumentException) {
            Log.e(TAG, "ServiceConnection already unbound")
        }
    }

    init {
        val intent = Intent(context, GattServerService::class.java)
        val success = context.bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE)
        if (success) {
            Log.d(TAG, "Service bound successfully")
        } else {
            Log.e(TAG, "Service binding failed")
        }
    }
}