//
// Created by ccxz8 on 2025-01-16.
//

#ifndef MACRO_CUBESERVICE_H
#define MACRO_CUBESERVICE_H

#include <opencv2/opencv.hpp>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <mutex>
#include "../jvm/JvmManager.h"
#include "../CaptureThread.h"

class CubeService : public FrameObserver, public JvmManager {
public:
    CubeService(JNIEnv* env, jobject javaInstance, AAssetManager* mgr);
    ~CubeService();

    void onFrameCapture(const cv::UMat& frame) override;
    cv::UMat getLastProcessedFrame() const;
    std::string getLastOcrText() const;
    bool isRerolled() const;

private:
    AAssetManager* mgr;

    cv::UMat CUBE_TL_TEMPLATE;
    cv::UMat CUBE_BR_TEMPLATE;
    cv::Point cube_tl;
    cv::Point cube_br;
    
    cv::UMat detectCubeRegion(const cv::UMat& frame);
    std::pair<cv::Point, cv::Point> single_match(const cv::UMat& image, const cv::UMat& templ);
    
    cv::UMat last_frame;
    std::string last_ocr_text;
    bool frame_empty;
    bool previous_frame_empty;
    
    void processFrame(const cv::UMat& frame);
    std::string performOcr(const cv::Mat& mat);

    mutable std::mutex mutex;
    cv::UMat readImageFromAssets(AAssetManager* mgr, const char* filename);
};

#endif //MACRO_CUBESERVICE_H
