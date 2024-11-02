package com.example.macro.grpc

import android.util.Log
import com.example.macro.macro.KeyboardMacro

class RestartService(private val keyboardMacro: KeyboardMacro): GrpcService() {

    init {
        nativeObj = nativeCreateObject(50052)
        Log.d(TAG, "restart service pointer $nativeObj")
    }

    fun cleanup() {
        nativeDestroyObject(nativeObj);
    }

    fun stopReplay() {
        keyboardMacro.stopReplay()
        Log.i(TAG, "macro stop request")
    }

    private external fun nativeCreateObject(port: Int): Long
    private external fun nativeDestroyObject(nativeObj: Long)

    companion object {
        var TAG = "InputService";
        init {
            System.loadLibrary("native_lib")
        }
    }
}