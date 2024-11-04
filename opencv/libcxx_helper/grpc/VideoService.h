//
// Created by ccxz8 on 2024-07-31.
//

#ifndef MACRO_VIDEOSERVICE_H
#define MACRO_VIDEOSERVICE_H

#include "grpcpp/grpcpp.h"
#include <android/log.h>
#include "grpc/video_service.grpc.pb.h"
#include "GrpcServer.h"
#include "../CaptureThread.h"

#define LOG_TAG "VideoGrpc"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

typedef void (*F_Start_Minimap)(const Empty* request, MinimapResponse* response);
typedef void (*F_Stream_Video)(const VideoRequest* request, grpc::ServerWriter<VideoFrame>* writer);


class VideoServiceImpl final : public GrpcServer, public VideoService::Service {
public:
    grpc::Status StreamVideo(grpc::ServerContext* context, const VideoRequest* request, grpc::ServerWriter<VideoFrame>* writer) override;

    grpc::Status StartMinimap(grpc::ServerContext* context, const Empty* request, MinimapResponse* response) override;

    VideoServiceImpl(JNIEnv* env, jobject instance, CaptureThread* pCaptureThread) {
        // Java 객체를 전역 참조로 저장
        this->captureThread = pCaptureThread;
        this->kotlinInstance = env->NewGlobalRef(instance);
    }
    void init(int port);
private :
    F_Start_Minimap startMinimapCallback;
    F_Stream_Video streamVideoCallback;

    CaptureThread* captureThread;
};


#endif //MACRO_VIDEOSERVICE_H
