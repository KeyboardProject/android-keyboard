package com.example.macro.service

import android.content.res.AssetManager
import android.graphics.Bitmap
import com.example.macro.capture.CaptureThread
import com.google.mlkit.vision.common.InputImage
import com.google.mlkit.vision.text.TextRecognition
import com.google.mlkit.vision.text.korean.KoreanTextRecognizerOptions
import kotlinx.coroutines.tasks.await
import java.nio.ByteBuffer

class CubeService(private val captureThread: CaptureThread, assets: AssetManager) {
    private val recognizer = TextRecognition.getClient(KoreanTextRecognizerOptions.Builder().build())
    private val assetManager: AssetManager = assets

    protected var nativeObj: Long = 0

    init {
        nativeObj = nativeCreateObject(assetManager, captureThread.getNativPrt())
    }

    fun getPointer(): Long {
        return nativeObj;
    }

    fun cleanup() {
        nativeDestroyObject(nativeObj);
    }
    
    // C++에서 호출할 메서드
    fun recognizeText(width: Int, height: Int, byteBuffer: ByteBuffer): String {
        val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        bitmap.copyPixelsFromBuffer(byteBuffer)
        
        val image = InputImage.fromBitmap(bitmap, 0)
        var resultText = ""
        
        recognizer.process(image)
            .addOnSuccessListener { text ->
                resultText = text.text
            }
            .addOnFailureListener { e ->
                resultText = ""
            }
        
        return resultText
    }

    private external fun nativeCreateObject(assetManager: AssetManager, captureThreadPtr: Long): Long
    private external fun nativeDestroyObject(nativeObj: Long)

    companion object {
        init {
            System.loadLibrary("native_lib")
        }
    }
}