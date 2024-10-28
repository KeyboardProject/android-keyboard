package com.example.macro.grpc

import android.util.Log

class InputService: GrpcService() {

    init {
        nativeObj = nativeCreateObject()
        Log.d("InputService", "capture thread pointer $nativeObj")
    }

    private fun startRequest(nativeObj: Long) {

    }

    private external fun nativeCreateObject(): Long
    private external fun nativeDestroyObject(nativeObj: Long)
    companion object {
        init {
            System.loadLibrary("native_lib")
        }
    }
}