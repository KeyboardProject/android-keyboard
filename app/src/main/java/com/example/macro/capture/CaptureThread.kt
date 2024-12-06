package com.example.macro.capture

import android.content.ContentValues
import android.content.Context
import android.content.res.AssetManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.os.Environment
import android.provider.MediaStore
import android.util.Log
import org.opencv.android.Utils
import org.opencv.core.Mat
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.nio.ByteBuffer

class CaptureThread(assets: AssetManager, val context: Context) {
    private var nativeObj: Long
    private val assetManager: AssetManager = assets

    init {
        nativeObj = nativeCreateObject(assetManager)
        Log.d("CaptureThread", "capture thread pointer $nativeObj")
    }

//    fun getVideo(): ByteBuffer {
//        return nativeGetVideo(nativeObj)
//    }

    fun getNativPrt(): Long {
        return nativeObj;
    }

    fun startMinimap() {
        nativeStartMinimap(nativeObj)
    }

    fun connectDevice(vid: Int, pid: Int, fd: Int, bus: Int, devAddr: Int, usbfs: String) {
        nativeConnectDevice(nativeObj, vid, pid, fd, bus, devAddr, usbfs)
    }

    @Throws(Throwable::class)
    protected fun finalize() {
        nativeDestroyObject(nativeObj)
    }

    fun loadBitmapFromAssets(filename: String): Bitmap? {
        return try {
            assetManager.open(filename).use { inputStream ->
                BitmapFactory.decodeStream(inputStream)
            }
        } catch (e: IOException) {
            e.printStackTrace()
            null
        }
    }



    private external fun nativeCreateObject(assetManager: AssetManager): Long
    private external fun nativeDestroyObject(nativeObj: Long)
    external fun nativeConnectDevice(nativeObj: Long, vendorId: Int, productId: Int, fileDescriptor: Int, busNum: Int, devAddr: Int, usbfs: String)
    external fun nativeStartMinimap(nativeObj: Long)

    external fun nativeGetMinimapData(nativeObj: Long): String?
    external fun nativeGetMinimapImage(nativeObj: Long): ByteArray?
    external fun nativeGetMinimapImageBase64(nativeObj: Long): String?

    fun saveMinimapToJson(filePath: String) {
        val jsonData = nativeGetMinimapData(nativeObj)

        if (jsonData == null) {
            Log.e("CaptureThread", "Failed to get minimap JSON data.")
            return
        }

        // JSON 파일 저장
        try {
            File(filePath).writeText(jsonData.toString())
            Log.d("CaptureThread", "Minimap data saved to JSON: $filePath")
        } catch (e: IOException) {
            Log.e("CaptureThread", "Failed to save JSON: ${e.message}")
        }
    }

    fun saveMinimapToImage(filePath: String) {
        val imageData = nativeGetMinimapImage(nativeObj)

        if (imageData == null) {
            Log.e("CaptureThread", "Failed to get image data.")
        } else {
            Log.d("CaptureThread", "Received image data size: ${imageData.size}")
            try {
                File(filePath).writeBytes(imageData)
                Log.d("CaptureThread", "Minimap image saved to: $filePath")
            } catch (e: IOException) {
                Log.e("CaptureThread", "Failed to save image: ${e.message}")
            }
        }

        // ByteArray를 파일로 저장

    }
    fun saveMinimapBase64ToImage(filePath: String) {
        val base64Data = nativeGetMinimapImageBase64(nativeObj)

        if (base64Data == null) {
            Log.e("CaptureThread", "Failed to get Base64 encoded image data.")
            return
        }

        try {
            // Base64 디코딩
            val decodedBytes = android.util.Base64.decode(base64Data, android.util.Base64.DEFAULT)

            // ByteArray를 파일로 저장
            File(filePath).writeBytes(decodedBytes)
            Log.d("CaptureThread", "Minimap image saved to: $filePath")
        } catch (e: IOException) {
            Log.e("CaptureThread", "Failed to save image: ${e.message}")
        }
    }



    companion object {
        init {
            System.loadLibrary("native_lib")
        }
    }
}
