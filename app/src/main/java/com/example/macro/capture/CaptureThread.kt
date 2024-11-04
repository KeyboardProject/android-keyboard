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

    companion object {
        init {
            System.loadLibrary("native_lib")
        }
    }
}
