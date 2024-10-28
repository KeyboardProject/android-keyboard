//
// Created by ccxz8 on 2024-08-29.
//

#ifndef MACRO_GRPCSERVER_H
#define MACRO_GRPCSERVER_H

#include "grpc/InputService.h"
#include "grpc/VideoService.h"


class GrpcServer {
public:
    GrpcServer(InputService *inputService);
    void inputLoadServer();
    void restartLoadServer();
    void videoLoadServer();

private:
    InputService *inputService;
    VideoServiceImpl *videoService;
};


#endif //MACRO_GRPCSERVER_H
