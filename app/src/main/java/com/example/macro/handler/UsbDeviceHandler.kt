package com.example.macro.handler

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.ServiceConnection
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbDeviceConnection
import android.hardware.usb.UsbManager
import android.os.IBinder
import android.util.Log
import com.example.macro.capture.CaptureThread
import com.example.macro.keyboard.GattServerService
import com.example.macro.macro.KeyboardMacro
import com.example.macro.test.UsbHelper

class UsbDeviceHandler(
    private val context: Context,
    private var captureThread: CaptureThread?,
    private val keyboardMacro: KeyboardMacro
) {
    private val usbManager = context.getSystemService(Context.USB_SERVICE) as UsbManager
    private val deviceProcessingMap = mutableMapOf<String, Boolean>()
    private val usbHelper = UsbHelper(context)
    private var gattServerService: GattServerService? = null

    private val usbReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
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
                            Log.d(TAG,"request permisson $it")
                            var validDevice = ""
                            validDevice = usbHelper.getUsbDeviceType(it);
                            if (validDevice == "CaptureBoard" || validDevice == "Keyboard") {
                                Log.d(TAG,"request permisson $validDevice")
                                usbManager.requestPermission(it, permissionIntent)
                            } else {
                                Log.e(TAG, "USB device is not valid. so don't request permission")
                            }

                        }
                    }
                }

                ACTION_USB_PERMISSION -> {
                    synchronized(this) {
                        val device = intent.getParcelableExtra<UsbDevice>(UsbManager.EXTRA_DEVICE)
                        Log.d(TAG, "USB permission result for device $device")
                        if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                            device?.apply {
                                Thread {
                                    var validDevice = ""
                                    for (i in 1..10) { // 최대 10번 시도
                                        validDevice = usbHelper.getUsbDeviceType(this);
                                        Thread.sleep(500) // 500ms 대기
                                    }

                                    if (validDevice == "CaptureBoard") {
                                        startCaptureDevice(device)
                                    } else if(validDevice == "Keyboard") {
                                        Log.d(TAG, "keyboard start");
                                        startListeningForKeyboard(device)
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
    }

    init {
        val filter = IntentFilter().apply {
            addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED)
            addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
            addAction(ACTION_USB_PERMISSION)
        }
        context.registerReceiver(usbReceiver, filter, Context.RECEIVER_NOT_EXPORTED)
    }

    // 키보드 장치 처리
    private fun startListeningForKeyboard(device: UsbDevice) {
        val connection: UsbDeviceConnection = usbManager.openDevice(device) ?: run {
            Log.e(TAG, "Failed to open USB Keyboard device")
            return
        }

        val usbInterface = device.getInterface(0)
        if (!connection.claimInterface(usbInterface, true)) {
            Log.e(TAG, "Failed to claim USB Keyboard interface")
            return
        }

        val endpoint = usbInterface.getEndpoint(0)
        val buffer = ByteArray(endpoint.maxPacketSize)

        Thread {
            while (true) {
                // bulkTransfer 호출 후 결과 출력
                val bytesRead = connection.bulkTransfer(endpoint, buffer, buffer.size, 5000)
                if (bytesRead > 0) {
                  val keyData = buffer.copyOf(bytesRead)
                    // 서비스로 키 이벤트 전달
                    val service = gattServerService
                    if (service != null) {
                        // 서비스의 sendReport() 메서드에 keyData 전달
                        service.sendReport(keyData)
                    } else {
                        Log.e(TAG, "GattServerService is not connected")
                    }

                    keyboardMacro.addInput(keyData)  // KeyboardMacro에 이벤트 추가
                }
            }
        }.start()
    }

    // 캡처 보드 장치 처리
    private fun startCaptureDevice(device: UsbDevice) {
        val fileDescriptor = usbHelper.getFileDescriptor(device)
        if (fileDescriptor != null) {
            Log.d(TAG, "Initializing USB capture")
            val vendorId = device.vendorId
            val productId = device.productId
            val busNum = usbHelper.getBusNum(device)
            val devAddr = usbHelper.getDevAddr(device)
            val usbfs = usbHelper.getUsbFs()
            Log.d(TAG, "$fileDescriptor")
            Log.d(TAG, "$vendorId $productId $busNum $devAddr $usbfs")
            Log.d(TAG, "capture thread null? $captureThread")
            captureThread?.connectDevice(vendorId, productId, fileDescriptor, busNum, devAddr, usbfs)
            Log.d(TAG, "Initializing End USB capture")
        }
    }

    fun cleanup() {
        // BroadcastReceiver 해제
        try {
            context.unregisterReceiver(usbReceiver)
            Log.d(TAG, "BroadcastReceiver unregistered")
        } catch (e: IllegalArgumentException) {
            Log.e(TAG, "BroadcastReceiver already unregistered")
        }

        // ServiceConnection 해제
        try {
            context.unbindService(serviceConnection)
            Log.d(TAG, "ServiceConnection unbound")
        } catch (e: IllegalArgumentException) {
            Log.e(TAG, "ServiceConnection already unbound")
        }
    }

    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            val binder = service as? GattServerService.LocalBinder
            if (binder != null) {
                gattServerService = binder.getService()
                Log.d(TAG, "Service connected")
            } else {
                Log.e(TAG, "Service binding failed")
            }
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            gattServerService = null
            Log.d(TAG, "Service disconnected")
        }
    }

    init {
        // 서비스 바인딩
        val intent = Intent(context, GattServerService::class.java)
        val success = context.bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE)
    }

    companion object {
        const val TAG = "UsbHandler"
        const val ACTION_USB_PERMISSION = "com.example.macro.USB_PERMISSION"
    }
}
