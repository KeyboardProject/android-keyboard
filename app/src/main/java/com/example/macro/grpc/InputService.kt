package com.example.macro.grpc

import android.util.Log

class InputService: GrpcService() {

    init {
        nativeObj = nativeCreateObject(50051)
        Log.d(TAG, "input service pointer $nativeObj")
    }

    private fun startRequest(fileName: String) {
        Log.d(TAG, "start request $fileName")
    }

    private fun stopRequest() {
        Log.d(TAG, "stop request")
    }

    private fun startReplayDebug(writerPtr: Long, fileName: String) {

    }



    private external fun nativeCreateObject(port: Int): Long
    private external fun nativeDestroyObject(nativeObj: Long)
    private external fun sendReplayKeyEvent(writerPtr: Long, eventDescription: String)
    companion object {
        var TAG = "InputService";
        init {
            System.loadLibrary("native_lib")
        }
    }
}