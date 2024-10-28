//
// Created by ccxz8 on 2024-07-31.
//

#include "VideoService.h"

grpc::Status VideoServiceImpl::StreamVideo(grpc::ServerContext* context, const VideoRequest* request, grpc::ServerWriter<VideoFrame>* writer) {
    LOGI("connect video stream");

    if (streamVideoCallback != NULL) {
        streamVideoCallback(request, writer);
    }

    return grpc::Status::OK;
}

grpc::Status VideoServiceImpl::StartMinimap(grpc::ServerContext* context, const Empty* request, MinimapResponse* response) {
    LOGI("connect start minimap");

    if (startMinimapCallback != NULL) {
        startMinimapCallback(request, response);
    }

    return grpc::Status::OK;
}