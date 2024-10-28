package com.example.macro

import android.media.MediaCodec
import android.media.MediaFormat
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.graphics.SurfaceTexture
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.os.ParcelFileDescriptor
import android.util.Log
import android.view.Surface
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import com.example.macro.capture.CaptureThread
import com.example.macro.test.UsbHelper
import java.io.FileDescriptor
import java.io.FileInputStream
import java.io.IOException

class UsbPermissionReceiver : BroadcastReceiver() {
    private var surfaceTexture: SurfaceTexture? = null
    private val deviceProcessingMap = mutableMapOf<String, Boolean>()

    override fun onReceive(context: Context, intent: Intent) {
        Log.d(TAG, "BroadcastReceiver onReceive called ${intent.action}")
        val usbManager = context.getSystemService(Context.USB_SERVICE) as UsbManager

        when (intent.action) {
            UsbManager.ACTION_USB_DEVICE_ATTACHED -> {
                val device = intent.getParcelableExtra<UsbDevice>(UsbManager.EXTRA_DEVICE)
                Log.d(TAG, "onReceive device")
                device?.let {
                    if (deviceProcessingMap[it.deviceName] != true) {
                        deviceProcessingMap[it.deviceName] = true
                        val permissionIntent = PendingIntent.getBroadcast(
                            context,
                            0,
                            Intent(ACTION_USB_PERMISSION).apply {
                                setPackage(context.packageName)
                            },
                            PendingIntent.FLAG_MUTABLE
                        )
                        usbManager.requestPermission(it, permissionIntent)
                    }
                }
            }

            ACTION_USB_PERMISSION -> {
                synchronized(this) {
                    val device = intent.getParcelableExtra<UsbDevice>(UsbManager.EXTRA_DEVICE)
                    Log.d(TAG, "USB permission result for device $device")
                    if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                        device?.apply {
                            val usbHelper = UsbHelper(context)
                            Thread {
                                var validDevice = false
                                for (i in 1..10) { // 최대 10번 시도
                                    if (usbHelper.isValidUsbDevice(this)) {
                                        validDevice = true
                                        break
                                    }
                                    Thread.sleep(500) // 500ms 대기
                                }

                                if (validDevice) {
                                    val fileDescriptor = usbHelper.getFileDescriptor(this)
                                    if (fileDescriptor != null && surfaceTexture != null) {
                                        Log.d(TAG, "Initializing USB capture")
                                        val vendorId = this.vendorId
                                        val productId = this.productId
                                        val busNum = usbHelper.getBusNum(this)
                                        val devAddr = usbHelper.getDevAddr(this)
                                        val usbfs = usbHelper.getUsbFs()
                                        Log.d(TAG, "$fileDescriptor")
                                        Log.d(TAG, "$vendorId $productId $busNum $devAddr $usbfs")
                                        Log.d(TAG, "capture thread null? $captureThread")
                                        captureThread?.connectDevice(vendorId, productId, fileDescriptor, busNum, devAddr, usbfs)
//                                        nativeConnectDevice(vendorId, productId, fileDescriptor, busNum, devAddr, usbfs)
                                        Log.d(TAG, "Initializing End USB capture")
                                        // initializeUsbCapture(fileDescriptor, surfaceTexture!!)
                                    }
                                } else {
                                    Log.e(TAG, "USB device is not valid or has no valid endpoints")
                                }
                            }.start()
                        }
                    } else {
                        Log.d(TAG, "Permission denied for device $device")
                    }
                    deviceProcessingMap[device?.deviceName ?: ""] = false
                }
            }
        }
    }

    public external fun nativeConnectDevice(vendorId: Int, productId: Int, fileDescriptor: Int, busNum: Int, devAddr: Int, usbfs: String)


    fun setSurfaceTexture(surfaceTexture: SurfaceTexture) {
        this.surfaceTexture = surfaceTexture
    }

    private var captureThread: CaptureThread? = null
    fun setCaptureThread(captureThread: CaptureThread) {
        this.captureThread = captureThread;
    }

    companion object {
        val TAG = "Macro USB"
        const val ACTION_USB_PERMISSION = "com.example.macro.USB_PERMISSION"
        const val ACTION_USB_PERMISSION_GRANTED = "com.example.macro.USB_PERMISSION_GRANTED"
        const val ACTION_USB_PERMISSION_DENIED = "com.example.macro.USB_PERMISSION_DENIED"
    }


}
