package com.example.macro.grpc

import androidx.compose.runtime.key
import com.example.macro.capture.CaptureThread
import com.example.macro.macro.KeyboardMacro

class GrpcMain(private val keyboardMacro: KeyboardMacro, private val captureThread: CaptureThread) {
    fun cleanup() {
        inputService.cleanup()
    }

    private var inputService: InputService
    private var restartService: RestartService
    private var videoService: VideoService

    init {
        videoService = VideoService(captureThread)
        inputService = InputService(keyboardMacro, captureThread)
        restartService = RestartService(keyboardMacro)
    }

    companion object {
        init {
            System.loadLibrary("native_lib")
        }
    }
}