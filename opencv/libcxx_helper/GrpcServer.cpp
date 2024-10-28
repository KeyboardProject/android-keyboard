//
// Created by ccxz8 on 2024-08-29.
//

#include <thread>
#include "GrpcServer.h"
#include <jni.h>
#include <iostream>
#include "grpcpp/grpcpp.h"

void GrpcServer::inputLoadServer() {
    std::thread inputThread([this]() {
        std::string server_address("0.0.0.0:50051");
        grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(this->inputService);
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        std::cout << "Server listening on " << server_address << std::endl;
        server->Wait();
    });

    inputThread.detach(); // 스레드를 분리하여 백그라운드에서 실행되도록 함
}

void GrpcServer::restartLoadServer() {

}

void GrpcServer::videoLoadServer() {

}

GrpcServer::GrpcServer(InputService *inputService) {
    this->inputService = inputService;
}


extern "C"
JNIEXPORT void JNICALL
Java_com_example_macro_grpc_GrpcMain_nativeRunGrpcServer(JNIEnv *env, jobject thiz) {

}
extern "C"
JNIEXPORT jlong JNICALL
Java_com_example_macro_grpc_GrpcMain_nativeCreateObject(JNIEnv *env, jobject thiz, jlong lInputService) {
    InputService *pInputService = reinterpret_cast<InputService *>(lInputService);

    GrpcServer *pGrpcServer = new GrpcServer(pInputService);
    pGrpcServer->inputLoadServer();
    return reinterpret_cast<jlong>(pGrpcServer);
}



extern "C"
JNIEXPORT void JNICALL
Java_com_example_macro_grpc_GrpcMain_nativeDestroyObject(JNIEnv *env, jobject thiz,
                                                         jlong native_obj) {
    delete reinterpret_cast<GrpcServer *>(native_obj);
}