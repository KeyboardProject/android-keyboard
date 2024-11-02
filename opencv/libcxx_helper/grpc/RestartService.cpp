//
// Created by ccxz8 on 2024-11-01.
//

#include "RestartService.h"
#include <jni.h>

grpc::Status RestartService::RestartProcess(grpc::ServerContext* context, const RestartRequest* request,
                                                RestartResponse* response) {
    std::string str = "매크로 중지";
    LOGI("%s", str.c_str());

    std::string methodName = "stopReplay";
    std::string methodSig = "()V";
    std::vector<JavaArg> args;

    callJavaMethod(methodName, methodSig, args);

    response->set_message("매크로 중지");
    return grpc::Status::OK;
}

grpc::Status RestartService::RequestUpdate(grpc::ServerContext* context, const UpdateRequest* request,
                                           grpc::ServerWriter<UpdateResponse>* writer) {

}

void RestartService::init(int port) {
    LOGI("Restart Service %d",port);
    initServer(port, this);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_example_macro_grpc_RestartService_nativeCreateObject(JNIEnv *env, jobject thiz,
                                                              jint port) {
    gJavaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    RestartService *pRestartService = new RestartService(env, thiz);
    pRestartService->init(port);
    return reinterpret_cast<jlong>(pRestartService);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_macro_grpc_RestartService_nativeDestroyObject(JNIEnv *env, jobject thiz,
                                                               jlong native_obj) {
    // TODO: implement nativeDestroyObject()
}