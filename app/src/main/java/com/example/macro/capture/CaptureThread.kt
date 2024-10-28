package com.example.macro.capture

import android.content.res.AssetManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.util.Log
import java.io.IOException
import java.nio.ByteBuffer

class CaptureThread(assets: AssetManager) {
    private var nativeObj: Long
    private val assetManager: AssetManager = assets

    init {
        nativeObj = nativeCreateObject(assetManager)
        Log.d("CaptureThread", "capture thread pointer $nativeObj")
    }

    fun getMinimap(): ByteBuffer {
        return nativeGetMinimap(nativeObj)
    }

    fun getVideo(): ByteBuffer {
        return nativeGetVideo(nativeObj)
    }

    fun calculateMinimap(): Boolean {
        return nativeCalculateMinimap(nativeObj)
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
    private external fun nativeGetMinimap(nativeObj: Long): ByteBuffer
    private external fun nativeGetVideo(nativeObj: Long): ByteBuffer
    private external fun nativeCalculateMinimap(nativeObj: Long): Boolean
    external fun nativeConnectDevice(nativeObj: Long, vendorId: Int, productId: Int, fileDescriptor: Int, busNum: Int, devAddr: Int, usbfs: String)
    external fun nativeStartMinimap(nativeObj: Long)

    companion object {
        init {
            System.loadLibrary("native_lib")
        }
    }
}
