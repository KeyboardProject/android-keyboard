package com.example.macro.grpc

import com.example.macro.macro.KeyboardMacro

class GrpcMain(private val keyboardMacro: KeyboardMacro) {
    private var inputService: InputService

    init {
        inputService = InputService(keyboardMacro)
    }

    companion object {
        init {
            System.loadLibrary("native_lib")
        }
    }
}