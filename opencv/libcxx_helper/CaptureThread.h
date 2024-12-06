//
// Created by ccxz8 on 2024-07-14.
//

#ifndef MACRO_CAPTURETHREAD_H
#define MACRO_CAPTURETHREAD_H

#include <opencv2/opencv.hpp>
#include <thread>
#include <chrono>
#include <iostream>
#include <mutex>
#include <atomic>
#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <shared_mutex>
#include "libuvc/libuvc.h"

#define LOG_TAG "CaptureThread"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

class CaptureSystemNotify {
public:
    virtual ~CaptureSystemNotify() = default;
    virtual void onCaptureSystemNotify(std::string message) = 0;
};

class CaptureThread {
public:
    std::mutex mtx;
    std::condition_variable cv;

    CaptureThread(AAssetManager* mgr);
    void start();
    void stop();
    void processFrame(const uint8_t* frameBuffer, int width, int height);
    void captureFrame(cv::UMat currentFrame);
    void stopMinimap();
    cv::UMat getMinimap();
    cv::UMat getVideo();
    void connectDevice(int vendor_id, int product_id,
                      int file_descriptor, int bus_num,
                      int dev_addr, const char * usbfs);

    cv::UMat convertFrame(uvc_frame_t *frame);

    cv::UMat getFrame();

    void initCaptureMinimap();

    void addObserver(CaptureSystemNotify* observer);
    void removeObserver(CaptureSystemNotify* observer);

    bool calculateMinimap();
private:
    AAssetManager* mgr;  // 추가
    cv::UMat frame;
    cv::UMat minimap;
    double minimap_ratio;
    std::shared_mutex frame_mutex;
    std::shared_mutex minimap_frame_mutex;
    std::thread captureThread;
    cv::Point mm_tl;
    cv::Point mm_br;
    bool ready;
    std::atomic_bool running;
    std::atomic_bool frameReady;

    uvc_device_t *mDevice;
    uvc_device_handle_t *mDeviceHandle;
    uvc_context_t *mContext;
    uvc_stream_ctrl_t ctrl;

    std::vector<CaptureSystemNotify*> observers;

    std::atomic_bool isCharacterDetectionActive;

    std::chrono::steady_clock::time_point lastNotifyTime;

    std::pair<cv::Point, cv::Point> single_match(const cv::UMat &image, const cv::UMat &templ);
    std::vector<cv::Point> multi_match(const cv::UMat &image, const cv::UMat &templ, double threshold);
    cv::UMat filterColor(const cv::UMat &img, const std::vector<std::pair<cv::Scalar, cv::Scalar>> &ranges);
    cv::Point2d convert_to_relative(const cv::Point &point, const cv::Size &size);

    static void frameCallback(uvc_frame_t *frame, void *ptr);

    void systemNotify(std::string message);

    void updateCharacterDetectionStatus(bool state);

    std::chrono::time_point<std::chrono::steady_clock> lastDetectionTime;
    const std::chrono::seconds detectionTimeout = std::chrono::seconds(2); // 3초 제한

    cv::UMat MM_TL_TEMPLATE;
    cv::UMat MM_BR_TEMPLATE;
    cv::UMat PLAYER_TEMPLATE;
    cv::UMat RUNE_TEMPLATE;
};

extern "C" {
JNIEXPORT jlong JNICALL Java_com_example_macro_capture_CaptureThread_nativeCreateObject(JNIEnv* env, jobject obj, jobject assetManager);
JNIEXPORT void JNICALL Java_com_example_macro_capture_CaptureThread_nativeDestroyObject(JNIEnv* env, jobject obj, jlong thiz);
JNIEXPORT jobject JNICALL Java_com_example_macro_capture_CaptureThread_nativeGetMinimap(JNIEnv* env, jobject obj, jlong thiz);
JNIEXPORT jobject JNICALL Java_com_example_macro_capture_CaptureThread_nativeGetVideo(JNIEnv* env, jobject obj, jlong thiz);
JNIEXPORT jboolean JNICALL Java_com_example_macro_capture_CaptureThread_nativeCalculateMinimap(JNIEnv* env, jobject obj, jlong thiz);
JNIEXPORT void JNICALL Java_com_example_macro_capture_CaptureThread_nativeStart(JNIEnv* env, jobject obj, jlong thiz);
JNIEXPORT void JNICALL Java_com_example_macro_capture_CaptureThread_nativeStop(JNIEnv* env, jobject obj, jlong thiz);
JNIEXPORT void JNICALL Java_com_example_macro_capture_CaptureThread_nativeProcessFrame(JNIEnv *env, jobject thiz,
                                                                                       jobject frame_buffer, jint width,
                                                                                       jint height);
}

#endif //MACRO_CAPTURETHREAD_H
