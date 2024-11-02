//
// Created by ccxz8 on 2024-11-01.
//

#ifndef MACRO_RESTARTSERVICE_H
#define MACRO_RESTARTSERVICE_H

#include <android/log.h>
#include "grpcpp/grpcpp.h"
#include "grpc/restart_service.grpc.pb.h"
#include "GrpcServer.h"
#include "../jvm/JvmManager.h"
#include "../jni_helper.h"
#include "../jvm/JvmManager.h"

class RestartService final : public GrpcServer, Restart::Service {
    grpc::Status RestartProcess(grpc::ServerContext* context, const RestartRequest* request,
                                RestartResponse* response) override;

    grpc::Status RequestUpdate(grpc::ServerContext* context, const UpdateRequest* request,
                               grpc::ServerWriter<UpdateResponse>* writer) override;
public:
    RestartService(JNIEnv* env, jobject instance) {
        // Java 객체를 전역 참조로 저장
        this->kotlinInstance = env->NewGlobalRef(instance);
    }
    void init(int port);
};


#endif //MACRO_RESTARTSERVICE_H
