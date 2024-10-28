package com.example.macro.grpc

abstract class GrpcService {
    protected var nativeObj: Long = 0

    fun getPointer(): Long {
        return nativeObj;
    }
}