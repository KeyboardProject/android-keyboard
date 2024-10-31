//
// Created by ccxz8 on 2024-08-29.
//

#ifndef MACRO_GRPCSERVER_H
#define MACRO_GRPCSERVER_H

#include <android/log.h>
#include <jni.h>
#include "grpcpp/grpcpp.h"
#include "../jvm/JvmManager.h"

#define GRPC_LOG_TAG "GrpcServer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, GRPC_LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, GRPC_LOG_TAG, __VA_ARGS__)

class GrpcServer: protected JvmManager {
public:
    virtual void initServer(int port, grpc::Service* service);
};


#endif //MACRO_GRPCSERVER_H
