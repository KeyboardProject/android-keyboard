package com.example.macro.grpc

import android.util.Log
import com.example.macro.capture.CaptureThread

class VideoService(private val captureThread: CaptureThread): GrpcService() {
    init {
        nativeObj = nativeCreateObject(50053, captureThread.getNativPrt())
        Log.d(TAG, "input service pointer $nativeObj")
    }

    private external fun nativeCreateObject(port: Int, captureThreadPtr: Long): Long
    private external fun nativeDestroyObject(nativeObj: Long)

    companion object {
        var TAG = "VideoService";
        init {
            System.loadLibrary("native_lib")
        }
    }
}