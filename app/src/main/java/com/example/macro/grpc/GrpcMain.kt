package com.example.macro.grpc

import android.util.Log

class GrpcMain {
    private var nativeObj: Long

    private var inputService: InputService

    init {
        inputService = InputService()

        nativeObj = nativeCreateObject(inputService.getPointer())
        Log.d("Grpcmain", "grpc main pointer $nativeObj")
    }

    @Throws(Throwable::class)
    protected fun finalize() {
        nativeDestroyObject(nativeObj)
    }

    private external fun nativeCreateObject(inputService: Long): Long
    private external fun nativeDestroyObject(nativeObj: Long)
    private external fun nativeRunGrpcServer()

    companion object {
        init {
            System.loadLibrary("native_lib")
        }
    }
}