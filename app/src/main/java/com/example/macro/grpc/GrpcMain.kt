package com.example.macro.grpc

import androidx.compose.runtime.key
import com.example.macro.macro.KeyboardMacro

class GrpcMain(private val keyboardMacro: KeyboardMacro) {
    fun cleanup() {
        inputService.cleanup()
    }

    private var inputService: InputService
    private var restartService: RestartService

    init {
        inputService = InputService(keyboardMacro)
        restartService = RestartService(keyboardMacro)
    }

    companion object {
        init {
            System.loadLibrary("native_lib")
        }
    }
}