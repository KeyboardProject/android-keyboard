//
// Created by ccxz8 on 2024-08-29.
//

#include <thread>
#include "GrpcServer.h"
#include <jni.h>
#include <iostream>
#include "grpcpp/grpcpp.h"

void GrpcServer::initServer(int port, grpc::Service* service) {
    std::thread serverThread([this, port, service]() {
        std::string server_address = "0.0.0.0:" + std::to_string(port);

        grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

        // 현재 클래스를 서비스로 등록 (파생 클래스에서 호출 시 그 파생 클래스의 서비스가 등록됨)
        builder.RegisterService(service);

        // CompletionQueue 추가
        auto cq = builder.AddCompletionQueue();
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        if (!server) {
            LOGE("Failed to start server on  %s", server_address.c_str());
            std::cerr << "Failed to start server on " << server_address << std::endl;
            return;
        }
        LOGI("Server listening on  %s", server_address.c_str());
        std::cout << "Server listening on " << server_address << std::endl;

        std::thread([cq = std::move(cq)]() mutable {
            void* tag;
            bool ok;
            while (cq->Next(&tag, &ok)) {
                if (ok) {
                    // Completion queue event handling (필요시 추가 작업 가능)
                }
            }
        }).detach();

        server->Wait();
    });

    serverThread.detach();
}
