//
// Created by ccxz8 on 2024-07-31.
//

#include "VideoService.h"
#include <opencv2/opencv.hpp>
#include <jni.h>

grpc::Status VideoServiceImpl::StreamVideo(grpc::ServerContext* context, const VideoRequest* request, grpc::ServerWriter<VideoFrame>* writer) {
    LOGI("connect video stream");

    while (!context->IsCancelled()) { // 클라이언트가 연결을 끊을 때까지 반복
        // 비디오 프레임 가져오기
        cv::UMat frame = captureThread->getVideo();

        if (frame.empty()) {
//            LOGE("Empty frame received from captureThread");
            continue;
        }

        // OpenCV 프레임을 JPEG 형식으로 인코딩
        std::vector<uchar> buffer;
        cv::imencode(".jpg", frame, buffer);

        // VideoFrame 메시지 생성
        VideoFrame videoFrame;
        videoFrame.set_frame(buffer.data(), buffer.size());

        // 클라이언트로 프레임 전송
        if (!writer->Write(videoFrame)) {
            LOGE("Failed to write video frame to client");
            break; // 전송 실패 시 스트리밍 중단
        }

        // 전송 속도 조절을 위한 짧은 대기 (예: 30 FPS에 맞추기 위해)
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    return grpc::Status::OK;
}
grpc::Status VideoServiceImpl::CalculateMinimap(grpc::ServerContext* context, const Empty* request, grpc::ServerWriter<VideoFrame>* writer) {
    captureThread->calculateMinimap();

    while (!context->IsCancelled()) {
        // 비디오 프레임 가져오기
        cv::UMat frame = captureThread->getMinimap();

        if (frame.empty()) {
        //            LOGE("Empty frame received from captureThread");
            continue;
        }

        // OpenCV 프레임을 JPEG 형식으로 인코딩
        std::vector<uchar> buffer;
        cv::imencode(".jpg", frame, buffer);

        // VideoFrame 메시지 생성
        VideoFrame videoFrame;
        videoFrame.set_frame(buffer.data(), buffer.size());

        // 클라이언트로 프레임 전송
        if (!writer->Write(videoFrame)) {
            LOGE("Failed to write video frame to client");
            break; // 전송 실패 시 스트리밍 중단
        }

        // 전송 속도 조절을 위한 짧은 대기 (예: 30 FPS에 맞추기 위해)
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
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

void VideoServiceImpl::init(int port) {
    initServer(port, this);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_example_macro_grpc_VideoService_nativeCreateObject(JNIEnv *env, jobject thiz, jint port,
                                                            jlong capture_thread_ptr) {
    JNIHepler::gJavaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    CaptureThread* pCaptureThread = reinterpret_cast<CaptureThread *>(capture_thread_ptr);
    VideoServiceImpl *pInputService = new VideoServiceImpl(env, thiz, pCaptureThread);
    pInputService->init(port);
    return reinterpret_cast<jlong>(pInputService);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_macro_grpc_VideoService_nativeDestroyObject(JNIEnv *env, jobject thiz,
                                                             jlong native_obj) {
    delete reinterpret_cast<VideoServiceImpl *>(native_obj);
}