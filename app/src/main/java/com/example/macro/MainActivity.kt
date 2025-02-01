package com.example.macro

import android.Manifest
import android.app.AlertDialog
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.SurfaceTexture
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.view.SurfaceHolder
import android.view.TextureView
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.viewinterop.AndroidView
import com.example.macro.capture.CaptureThread
import com.example.macro.grpc.GrpcMain
import com.example.macro.handler.UsbDeviceHandler
import com.example.macro.handler.UsbDeviceHandler.Companion
import com.example.macro.keyboard.GattServerService
import com.example.macro.macro.KeyboardMacro
import com.example.macro.service.CubeService
import com.example.macro.test.UsbHelper
import com.example.macro.ui.theme.MacroTheme
import org.opencv.android.Utils
import org.opencv.core.CvType
import org.opencv.core.Mat
import java.io.File
import java.io.IOException
import java.nio.ByteBuffer


class MainActivity : ComponentActivity(), SurfaceHolder.Callback {
    private var usbKeyboardHandler: UsbDeviceHandler? = null
    private lateinit var captureThread: CaptureThread

    private lateinit var textureView: TextureView

    private lateinit var grpcMain: GrpcMain;

    private val keyboardMacro by lazy { KeyboardMacro.getInstance(this) }

    lateinit var permissionLauncher: ActivityResultLauncher<Array<String>>
    lateinit var usbHelper: UsbHelper;
    lateinit var cubeService: CubeService;

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MacroTheme {
                Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
                    Greeting("Android")
                    AndroidView(
                        factory = { context ->
                            TextureView(context).apply {
                                textureView = this
                                surfaceTextureListener = object : TextureView.SurfaceTextureListener {
                                    override fun onSurfaceTextureAvailable(surface: SurfaceTexture, width: Int, height: Int) {

                                    }

                                    override fun onSurfaceTextureSizeChanged(surface: SurfaceTexture, width: Int, height: Int) {}
                                    override fun onSurfaceTextureDestroyed(surface: SurfaceTexture): Boolean = true
                                    override fun onSurfaceTextureUpdated(surface: SurfaceTexture) {}
                                }
                            }
                        },
                        modifier = Modifier.fillMaxSize()
                    )
//                    AndroidView(
//                        factory = { context ->
//                            SurfaceView(context).apply {
//                                holder.addCallback(this@MainActivity)  // MainActivity를 Callback으로 등록
//                            }
//                        },
//                        modifier = Modifier.fillMaxSize()
//                    )
                    MinimapButton {
                        // Downloads 폴더에 저장하는 함수
                        fun saveJsonToDownloads(filePath: String, fileName: String) {
                            val downloadsDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
                            if (!downloadsDir.exists()) {
                                downloadsDir.mkdirs()
                            }
                            val file = File(downloadsDir, fileName)

                            try {
                                val jsonData = File(filePath).readText()
                                file.writeText(jsonData)
                                Toast.makeText(this, "File saved to Downloads: ${file.absolutePath}", Toast.LENGTH_SHORT).show()
                            } catch (e: IOException) {
                                Log.e("CaptureThread", "Failed to save JSON to Downloads: ${e.message}")
                            }
                        }

//                        if (captureThread != null) {
//                            Thread {
//                                val filePath = "${getExternalFilesDir(null)?.absolutePath}/minimap.json"
//                                captureThread?.saveMinimapToJson(filePath)
//
//                                runOnUiThread {
//
//                                    Toast.makeText(this, "Minimap saved to $filePath", Toast.LENGTH_SHORT).show()
//                                    saveJsonToDownloads(filePath,"minimap.json")
//
//                                }
//                            }.start()
//                        } else {
//                            Toast.makeText(this, "CaptureThread is not initialized", Toast.LENGTH_SHORT).show()
//                        }
                        if (captureThread != null) {
                            Thread {
                                val filePath = "${getExternalFilesDir(null)?.absolutePath}/minimap.txt"
                                captureThread?.saveMinimapToJson(filePath)

                                runOnUiThread {

                                    Toast.makeText(this, "Minimap saved to $filePath", Toast.LENGTH_SHORT).show()
                                    saveJsonToDownloads(filePath,"minimap.txt")

                                }
                            }.start()
                        } else {
                            Toast.makeText(this, "CaptureThread is not initialized", Toast.LENGTH_SHORT).show()
                        }
                    }

                }
            }
        }

        permissionLauncher = registerForActivityResult(
            ActivityResultContracts.RequestMultiplePermissions()
        ) { permissions ->
            val deniedPermissions = permissions.filter { !it.value }.map { it.key }

            if (deniedPermissions.isEmpty()) {
                // 모든 권한이 허용되었으므로 초기화 진행
                initialize()
            } else {
                // 권한이 거부된 경우
                Log.e(TAG, "Required permissions are denied: $deniedPermissions")
                // 필요에 따라 사용자에게 권한이 필요한 이유를 설명하고 재요청하거나, 앱을 종료할 수 있습니다.
                showPermissionDeniedDialog(deniedPermissions)
            }
        }

        captureThread = CaptureThread(assets, this)

        usbKeyboardHandler = UsbDeviceHandler(this, captureThread, keyboardMacro)

        grpcMain = GrpcMain(keyboardMacro, captureThread)

        usbHelper = UsbHelper(this)

        cubeService = CubeService(captureThread, assets)

        initializeUsbDevice()
        checkAndRequestPermissions()

    }

    private fun showPermissionDeniedDialog(deniedPermissions: List<String>) {
        val message = "다음 권한이 필요합니다:\n" + deniedPermissions.joinToString("\n") { getPermissionLabel(it) }

        AlertDialog.Builder(this)
            .setTitle("권한 필요")
            .setMessage(message)
            .setPositiveButton("권한 설정") { _, _ ->
                // 권한 재요청
                permissionLauncher.launch(deniedPermissions.toTypedArray())
            }
            .setNegativeButton("종료") { _, _ ->
                // 앱 종료 또는 다른 처리
                finish()
            }
            .show()
    }

    private fun getPermissionLabel(permission: String): String {
        return when (permission) {
            Manifest.permission.BLUETOOTH_SCAN -> "블루투스 스캔 권한"
            Manifest.permission.BLUETOOTH_CONNECT -> "블루투스 연결 권한"
            Manifest.permission.BLUETOOTH_ADVERTISE -> "블루투스 광고 권한"
            Manifest.permission.POST_NOTIFICATIONS -> "알림 권한"
            Manifest.permission.FOREGROUND_SERVICE_CONNECTED_DEVICE -> "포그라운드 서비스 연결된 장치 권한"
            else -> permission
        }
    }

    private fun initialize() {
        val serviceIntent = Intent(
            this,
            GattServerService::class.java
        )
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(serviceIntent)
        } else {
            startService(serviceIntent)
        }
    }

    private fun checkAndRequestPermissions() {
        val requiredPermissions = mutableListOf<String>()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            // Android 12 이상
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
                requiredPermissions.add(Manifest.permission.BLUETOOTH_SCAN)
            }
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                requiredPermissions.add(Manifest.permission.BLUETOOTH_CONNECT)
            }
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_ADVERTISE) != PackageManager.PERMISSION_GRANTED) {
                requiredPermissions.add(Manifest.permission.BLUETOOTH_ADVERTISE)
            }
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            // Android 13 이상
            if (checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                requiredPermissions.add(Manifest.permission.POST_NOTIFICATIONS)
            }
        }

        if (requiredPermissions.isNotEmpty()) {
            // 권한 요청
            permissionLauncher.launch(requiredPermissions.toTypedArray())
        } else {
            // 모든 권한이 허용되었으므로 초기화 진행
            initialize()
        }
    }

    private fun initializeUsbDevice() {
        val usbManager = getSystemService(Context.USB_SERVICE) as UsbManager
        val deviceList = usbManager.deviceList
        val deviceIterator: Iterator<UsbDevice> = deviceList.values.iterator()

        if (deviceIterator.hasNext()) {
            val device = deviceIterator.next()
            val permissionIntent = PendingIntent.getBroadcast(
                this,
                0,
                Intent(UsbDeviceHandler.ACTION_USB_PERMISSION).apply {
                    setPackage(packageName)
                },
                PendingIntent.FLAG_MUTABLE
            )
            var validDevice = ""
            validDevice = usbHelper.getUsbDeviceType(device);
            if (validDevice == "CaptureBoard" || validDevice == "Keyboard") {
                Log.d(UsbDeviceHandler.TAG,"request permisson $validDevice")
                usbManager.requestPermission(device, permissionIntent)
            } else {
                Log.e(UsbDeviceHandler.TAG, "USB device is not valid. so don't request permission")
            }
        }
    }

    override fun onStart() {
        super.onStart()

    }

    override fun surfaceCreated(holder: SurfaceHolder) {
//        Log.d(TAG, "surface")
//        Thread {
//            while (true) {
//                val frameBuffer = captureThread.getVideo()
//                if (frameBuffer == null) continue  // 프레임이 없는 경우 건너뛰기
//
//                val bitmap = byteBufferToBitmap(frameBuffer)
//                val canvas = holder.lockCanvas()
//                canvas?.drawBitmap(bitmap, 0f, 0f, null)
//                holder.unlockCanvasAndPost(canvas)
//                Thread.sleep(100) // Adjust this delay to match your capture frame rate
//            }
//        }.start()
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        // Handle surface changes if needed
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        // Handle surface destruction if needed
    }

    private fun byteBufferToBitmap(buffer: ByteBuffer): Bitmap {
        val mat = Mat(720, 1280, CvType.CV_8UC4)
        val byteArray = ByteArray(buffer.remaining()) // ByteBuffer의 데이터를 저장할 ByteArray 생성
        buffer.get(byteArray) // ByteBuffer에서 ByteArray로 데이터 복사
        mat.put(0, 0, byteArray) // Mat에 데이터를 설정

        val bitmap = Bitmap.createBitmap(mat.cols(), mat.rows(), Bitmap.Config.ARGB_8888)
        Utils.matToBitmap(mat, bitmap)
        return bitmap
    }

    companion object {
        val TAG = "MainActivity"
        const val ACTION_USB_PERMISSION = "com.example.macro.USB_PERMISSION"
    }

    override fun onDestroy() {
        // UsbHandler의 리소스 해제 메서드 호출
        usbKeyboardHandler?.cleanup()
        keyboardMacro.cleanup()
        grpcMain.cleanup()

        super.onDestroy()
    }
}

@Composable
fun Greeting(name: String, modifier: Modifier = Modifier) {
    Text(
        text = "Hello $name!",
        modifier = modifier
    )
}

@Preview(showBackground = true)
@Composable
fun GreetingPreview() {
    MacroTheme {
        Greeting("Android")
    }
}

@Composable
fun MinimapButton(onClick: () -> Unit) {
    Button(onClick = onClick, modifier = Modifier.fillMaxSize()) {
        Text("Start Minimap")
    }
}

