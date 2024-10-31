package com.example.macro.grpc

import android.util.Log

class GrpcMain {
    private var inputService: InputService

    init {
        inputService = InputService()
    }

    companion object {
        init {
            System.loadLibrary("native_lib")
        }
    }
}