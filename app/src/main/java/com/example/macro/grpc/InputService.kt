package com.example.macro.grpc

import android.util.Log
import com.example.macro.capture.CaptureThread
import com.example.macro.macro.ComplexReplayRequest
import com.example.macro.macro.KeyEvent
import com.example.macro.macro.KeyboardMacro

class InputService(private val keyboardMacro: KeyboardMacro, private val captureThread: CaptureThread): GrpcService() {

    init {
        nativeObj = nativeCreateObject(50051, captureThread.getNativPrt())
        Log.d(TAG, "input service pointer $nativeObj")
    }

    private fun startRequest(fileName: String) {
        Log.d(TAG, "start request $fileName")
        keyboardMacro.startRecord(fileName)
    }

    private fun stopRequest() {
        Log.d(TAG, "stop request")
        keyboardMacro.stopRecord()
    }

    private fun startReplayDebug(writerPtr: Long, fileName: String) {
        keyboardMacro.startReplayDebug(fileName) { event ->
            // 이벤트의 설명 문자열을 생성
            val eventDescription = "Event at ${event.delay}ns"

            // gRPC를 통해 KeyEvent 전송
            sendReplayKeyEvent(writerPtr, eventDescription)
        }
    }

    private fun getMacroDetail(fileName: String): List<KeyEvent> {
        var events = keyboardMacro.loadRecordedFile(fileName);

        return events;
    }

    private fun getFileList(): List<String> {
        return keyboardMacro.getFileList();
    }

    private fun startComplexReplay(request: ComplexReplayRequest) {
        keyboardMacro.startComplexReplay(request)
    }

    fun cleanup() {
        nativeDestroyObject(nativeObj);
    }

    private fun stopReplay() {
        Log.d(TAG, "stop replay")
        keyboardMacro.stopReplay();
    }

    fun handleRemoteKeyEvent(data: ByteArray) {
        // HID 리포트 데이터 처리
        // 예: 키보드 입력 시뮬레이션
        // HID 리포트를 해석하여 실제 키보드 입력을 발생시키는 로직 구현
//        Log.d("test","Received HID Report: ${data.joinToString(", ") { it.toUByte().toString() }}")

        // 예시: HID 리포트를 해석하여 키 이벤트 발생
        // 실제 구현은 HID 리포트 구조에 따라 다릅니다

        keyboardMacro.handleKeyboardInput(data);
    }

    private fun handleRemoteMouseEvent(absoluteX: Int, absoluteY: Int, wheelDelta: Int, leftButton: Boolean, rightButton: Boolean, middleButton: Boolean) {
        keyboardMacro.handleMoustInput(absoluteX, absoluteY, wheelDelta, leftButton, rightButton, middleButton)
    }

    private external fun nativeCreateObject(port: Int, captureThreadPtr: Long): Long
    private external fun nativeDestroyObject(nativeObj: Long)
    private external fun sendReplayKeyEvent(writerPtr: Long, eventDescription: String)
    companion object {
        var TAG = "InputService";
        init {
            System.loadLibrary("native_lib")
        }
    }

    // ImportProfile 구현
    private fun importProfile(fileName: String, data: ByteArray) {
        try {
            // 바이트 배열을 직접 KeyboardMacro로 전달
            Log.d(TAG, "Importing profile data: ${data.size} bytes");
            keyboardMacro.importProfile(fileName, data)
        } catch (e: Exception) {
            Log.e(TAG, "ImportProfile 실패: ${e.message}")
        }
    }

    // ExportProfile 구현 
    private fun exportProfile(fileName: String): List<KeyEvent> {
        try {
            // KeyboardMacro에서 KeyEvent 리스트를 받아서 반환
            return keyboardMacro.exportProfile(fileName)
        } catch (e: Exception) {
            throw RuntimeException("프로필 내보내기 실패: ${e.message}")
        }
    }

    private fun deleteMacros(fileNames: List<String>) {
        Log.d(TAG, "매크로 파일 삭제 요청: ${fileNames.joinToString()}")
        keyboardMacro.deleteMacros(fileNames)
    }
}