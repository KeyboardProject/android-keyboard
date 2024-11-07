#include "CaptureThread.h"
#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/bitmap.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include "UVCCamera/UVCCamera.h"

cv::UMat readImageFromAssets(AAssetManager* mgr, const char* filename) {
    AAsset* asset = AAssetManager_open(mgr, filename, AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("Error: Could not open asset %s", filename);
        return cv::UMat();
    }

    size_t size = AAsset_getLength(asset);
    std::vector<char> buffer(size);
    AAsset_read(asset, buffer.data(), size);
    AAsset_close(asset);

    cv::Mat img = cv::imdecode(cv::Mat(buffer), cv::IMREAD_GRAYSCALE);
    if (img.empty()) {
        LOGE("Error: Could not decode image %s", filename);
    }
    return img.getUMat(cv::ACCESS_READ);
}

CaptureThread::CaptureThread(AAssetManager* mgr) : mgr(mgr) {
    LOGI("CaptureThread initialized");

    MM_TL_TEMPLATE = readImageFromAssets(mgr, "minimap_tl_template.png");
    MM_BR_TEMPLATE = readImageFromAssets(mgr, "minimap_br_template.png");
    PLAYER_TEMPLATE = readImageFromAssets(mgr, "player_template.png");

    ready = false;
    updateCharacterDetectionStatus(false);

    mDeviceHandle = NULL;
}

void CaptureThread::start() {
    if (!running) {
        running = true;
    }
}

void CaptureThread::stop() {
    running = false;
}

void CaptureThread::addObserver(CharacterDetectionObserver* observer) {
    observers.push_back(observer);
}

void CaptureThread::removeObserver(CharacterDetectionObserver* observer) {
    observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
}

void CaptureThread::notifyObservers() {
    for (auto observer : observers) {
        observer->onCharacterDetectionStatusChanged(isCharacterDetectionActive()); // 새로운 메서드로 변경
    }
}

void CaptureThread::updateCharacterDetectionStatus(bool status) {
    _isCharacterDetectionActive = status;
    notifyObservers();  // 기존 옵저버 알림
}

cv::UMat CaptureThread::getMinimap() {
    std::shared_lock<std::shared_mutex> lock(minimap_frame_mutex);
    return minimap.clone();
}

cv::UMat CaptureThread::getVideo() {
    std::shared_lock<std::shared_mutex> lock(frame_mutex);
    return frame.clone();
}

void CaptureThread::processFrame(const uint8_t* frameBuffer, int width, int height) {
    cv::Mat yuv(height + height / 2, width, CV_8UC1, const_cast<uint8_t*>(frameBuffer));
    cv::UMat bgr;
    cv::cvtColor(yuv, bgr, cv::COLOR_YUV2BGR_NV21);

    {
        std::unique_lock<std::shared_mutex> lock(frame_mutex); // Write lock
        frame = bgr.clone();
        frameReady = true;
    }
}

void CaptureThread::stopMinimap(){
    updateCharacterDetectionStatus(false);
}

void CaptureThread::captureFrame(cv::UMat currentFrame) {
    {
        std::unique_lock<std::shared_mutex> lock(frame_mutex, std::try_to_lock);
        if (lock.owns_lock()) {
            frame = currentFrame.clone();
            frameReady = true;
        }
    }

    if (mm_tl.x < 0 || mm_tl.y < 0 || mm_br.x > frame.cols || mm_br.y > frame.rows || mm_tl.x > mm_br.x || mm_tl.y > mm_br.y) {
        LOGE("Error: Minimap coordinates are out of bounds");
        return;
    }

    cv::UMat minimap_frame = currentFrame(cv::Rect(mm_tl, mm_br));

    auto now = std::chrono::steady_clock::now();

    if(isCharacterDetectionActive()) {
        std::vector<cv::Point> player_points = multi_match(minimap_frame, PLAYER_TEMPLATE, 0.8);
        if (!player_points.empty()) {
            cv::Point2d player_pos = convert_to_relative(player_points[0], minimap_frame.size());
//            LOGI("Player position1: %f %f", player_pos.x, player_pos.y);
            lastDetectionTime = now;
        }
        else {
            if (now - lastDetectionTime > detectionTimeout) {
                updateCharacterDetectionStatus(false);
            }
        }
    }

    {
        std::unique_lock<std::shared_mutex> lock(minimap_frame_mutex);
        minimap = minimap_frame.clone();
    }

    if (!ready) {
        ready = true;
    }
}

void CaptureThread::initCaptureMinimap() {
    {
        lastDetectionTime = std::chrono::steady_clock::now();
        updateCharacterDetectionStatus(true);
    }
}

bool CaptureThread::calculateMinimap() {
    const int PT_WIDTH = PLAYER_TEMPLATE.cols;
    const int PT_HEIGHT = PLAYER_TEMPLATE.rows;

    const int MINIMAP_TOP_BORDER = 5;
    const int MINIMAP_BOTTOM_BORDER = 9;

    cv::UMat currentFrame;
    {
        if (frameReady) {
            {
                std::shared_lock<std::shared_mutex> lock(frame_mutex); // Read lock
                if (frameReady) {
                    currentFrame = frame.clone();
                } else {
                    std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait if frame not ready
                    return false;
                }
            }

            if (currentFrame.empty()) {
                LOGE("[!] Failed to grab frame");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                return false;
            }

            auto [tl, tl1] = single_match(currentFrame, MM_TL_TEMPLATE);
            auto [br2, br] = single_match(currentFrame, MM_BR_TEMPLATE);
            LOGI("Top-left corner: %d, %d, Bottom-right corner: %d, %d", tl.x, tl.y, br.x, br.y);

            mm_tl = cv::Point(tl.x + MINIMAP_BOTTOM_BORDER, tl.y + MINIMAP_TOP_BORDER);
            mm_br = cv::Point(std::max(mm_tl.x + PT_WIDTH, br.x - MINIMAP_BOTTOM_BORDER),
                              std::max(mm_tl.y + PT_HEIGHT, br.y - MINIMAP_BOTTOM_BORDER));

            if (mm_tl.x < 0 || mm_tl.y < 0 || mm_br.x > frame.cols || mm_br.y > frame.rows || mm_tl.x > mm_br.x || mm_tl.y > mm_br.y) {
                LOGE("Error: Minimap coordinates are out of bounds");
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                return false;
            }

            minimap_ratio = static_cast<double>(mm_br.x - mm_tl.x) / (mm_br.y - mm_tl.y);
            cv::UMat minimap_frame = currentFrame(cv::Rect(mm_tl, mm_br));

            {
                std::unique_lock<std::shared_mutex> lock(minimap_frame_mutex);
                minimap = minimap_frame.clone();
            }
        }
    }

    return true;
}

std::pair<cv::Point, cv::Point> CaptureThread::single_match(const cv::UMat &image, const cv::UMat &templ) {
    cv::UMat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

    cv::UMat result;
    cv::matchTemplate(gray, templ, result, cv::TM_CCOEFF);

    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

    int w = templ.cols;
    int h = templ.rows;

    cv::Point top_left = maxLoc;
    cv::Point bottom_right = cv::Point(top_left.x + w, top_left.y + h);
    return {top_left, bottom_right};
}

std::vector<cv::Point> CaptureThread::multi_match(const cv::UMat &image, const cv::UMat &templ, double threshold) {
    if (templ.rows > image.rows || templ.cols > image.cols) {
        return std::vector<cv::Point>();
    }

    cv::UMat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

    cv::UMat result;
    cv::matchTemplate(gray, templ, result, cv::TM_CCOEFF_NORMED);

    cv::UMat mask;
    cv::threshold(result, mask, threshold, 1.0, cv::THRESH_BINARY);

    std::vector<cv::Point> locations;
    cv::findNonZero(mask, locations);

    std::vector<cv::Point> results;
    for (const auto& loc : locations) {
        int x = static_cast<int>(round(loc.x + templ.cols / 2.0));
        int y = static_cast<int>(round(loc.y + templ.rows / 2.0));
        results.emplace_back(x, y);
    }

    return results;
}

cv::Point2d CaptureThread::convert_to_relative(const cv::Point &point, const cv::Size &size) {
    double x = static_cast<double>(point.x) / size.width;
    double y = static_cast<double>(point.y) / this->minimap_ratio / size.height;
    return cv::Point2d(x, y);
}

cv::UMat CaptureThread::convertFrame(uvc_frame_t *frame) {
    int width = frame->width;
    int height = frame->height;
    int frame_forUMat = frame->frame_format;

    if (frame_forUMat == UVC_FRAME_FORMAT_YUYV) {
        // YUYV 데이터를 BGR로 변환
        cv::Mat yuyvMat(height, width, CV_8UC4, frame->data);
        cv::UMat bgrUMat;
        cv::cvtColor(yuyvMat, bgrUMat, cv::COLOR_YUV2BGR_YUYV);

        return bgrUMat;
    } else if (frame_forUMat == UVC_FRAME_FORMAT_MJPEG) {
        std::vector<uchar> jpegData((uchar*)frame->data, (uchar*)frame->data + frame->data_bytes);
        cv::Mat jpegUMat = cv::imdecode(jpegData, cv::IMREAD_COLOR);
        if (jpegUMat.empty()) {
            LOGE("Failed to decode MJPEG frame\n");
            return cv::UMat();
        }

        cv::UMat bgrUMat;
        jpegUMat.copyTo(bgrUMat);

        return bgrUMat;
    } else {
        LOGE("Unsupported frame forUMat: %d\n", frame_forUMat);
        return cv::UMat();
    }
}

void CaptureThread::frameCallback(uvc_frame_t *frame, void *ptr) {
    CaptureThread *pCaptureThread = dynamic_cast<CaptureThread *>(static_cast<CaptureThread *>(ptr));


    if (pCaptureThread) {
        cv::UMat bgrUMat = pCaptureThread->convertFrame(frame);
        pCaptureThread->captureFrame(bgrUMat);
    } else {
        LOGE("Invalid CaptureThread instance\n");
    }
}

void CaptureThread::connectDevice(int vendor_id, int product_id, int file_descriptor, int bus_num,
                                 int dev_addr, const char *usbfs) {

    uvc_error_t result = UVC_ERROR_BUSY;

    uvc_device_t *_mDevice;
    uvc_device_handle_t *_mDeviceHandle = NULL;
    uvc_context_t *_mContext = NULL;
    uvc_stream_ctrl_t _ctrl;

    if (!_mDeviceHandle && file_descriptor) {
        if (UNLIKELY(!_mContext)) {
            result = uvc_init2(&_mContext, NULL, usbfs);
            LOGI("uvc_init2 %d", result);
            if (UNLIKELY(result < 0)) {
                LOGD("failed to init libuvc");
                return;
            }
        }
        file_descriptor = dup(file_descriptor);
        LOGI("fd test: %d",file_descriptor);
        LOGI("vendor %d", vendor_id);
        LOGI("product %d", product_id);
        LOGI("bus %d", bus_num);
        LOGI("dev %d", dev_addr);


        result = uvc_get_device_with_fd(_mContext, &_mDevice, vendor_id, product_id, NULL, file_descriptor, bus_num, dev_addr);
        if (LIKELY(!result)) {
            result = uvc_open(_mDevice, &_mDeviceHandle);
            if (LIKELY(!result)) {
                uvc_get_stream_ctrl_format_size(
                        _mDeviceHandle, &_ctrl, /* 네고셔블한 스트림 파라미터 */
                        UVC_FRAME_FORMAT_YUYV, /* 프레임 포맷 */
                        1280, 720, 30 /* 해상도와 프레임 속도 */
                );
                uvc_error_t result = uvc_start_streaming_bandwidth(
                        _mDeviceHandle, &_ctrl, CaptureThread::frameCallback, (void *)this, 1.0f, 0);
                LOGI("connect device");

            } else {
                // open出来なかった時
                LOGE("could not open camera:err=%d", result);
                uvc_unref_device(_mDevice);
                _mDevice = NULL;
                _mDeviceHandle = NULL;
                close(file_descriptor);

                return;
            }
        } else {
            LOGE("could not find camera:err=%d", result);
            close(file_descriptor);
            return;
        }
    } else {
        // カメラが既にopenしている時
        LOGW("camera is already opened. you should release first");
        return;
    }

    return;
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_example_macro_capture_CaptureThread_nativeCreateObject(JNIEnv *env, jobject obj,
                                                                jobject assetManager) {
    LOGI("creat object");
    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
    CaptureThread *captureThread = new CaptureThread(mgr);
    LOGI("create %p", captureThread);
    return reinterpret_cast<jlong>(captureThread);
}

JNIEXPORT void JNICALL
Java_com_example_macro_capture_CaptureThread_nativeDestroyObject(JNIEnv *env, jobject obj,
                                                                 jlong nativeObj) {
    delete reinterpret_cast<CaptureThread *>(nativeObj);
}

JNIEXPORT void JNICALL
Java_com_example_macro_capture_CaptureThread_nativeConnectDevice(JNIEnv *env, jobject thiz,
                                                                 jlong nativeObj, jint vendor_id,
                                                                 jint product_id, jint file_descriptor,
                                                                 jint bus_num, jint dev_addr, jstring usbfs) {
    const char *usbfs_str = env->GetStringUTFChars(usbfs, 0);
    CaptureThread *captureThread = reinterpret_cast<CaptureThread *>(nativeObj);

    LOGI("connect %p", captureThread);
    captureThread->connectDevice(vendor_id, product_id, file_descriptor, bus_num, dev_addr, usbfs_str);

    env->ReleaseStringUTFChars(usbfs, usbfs_str);
}

JNIEXPORT void JNICALL
Java_com_example_macro_capture_CaptureThread_nativeStartMinimap(JNIEnv *env, jobject thiz,
                                                                jlong nativeObj) {
    CaptureThread *captureThread = reinterpret_cast<CaptureThread *>(nativeObj);
    captureThread->initCaptureMinimap();
}

}
