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
    RUNE_TEMPLATE = readImageFromAssets(mgr, "rune_template.png");

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

void CaptureThread::addObserver(CaptureSystemNotify* observer) {
    observers.push_back(observer);
}

void CaptureThread::removeObserver(CaptureSystemNotify* observer) {
    observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
}

void CaptureThread::systemNotify(std::string message) {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastNotifyTime);

    if (duration.count() < 3000) {
        return;
    }

    // 알림 전송 및 시간 기록
    for (auto observer : observers) {
        observer->onCaptureSystemNotify(message);
    }
    lastNotifyTime = now;
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

void CaptureThread::updateCharacterDetectionStatus(bool state) {
    isCharacterDetectionActive = state;
}

void CaptureThread::stopMinimap(){
    updateCharacterDetectionStatus(false);
}

double calculate_distance(const cv::Point& point1, const cv::Point& point2) {
    return cv::norm(point1 - point2);
}

std::vector<cv::Point> find_color(const cv::Mat& minimap_frame, const cv::Vec3b& target_color, double threshold = 10.0) {
    // 이미지와 타겟 색상을 float32로 변환
    cv::Mat minimap_frame_float;
    minimap_frame.convertTo(minimap_frame_float, CV_32FC3);

    // 타겟 색상을 float32로 변환하고 cv::Scalar로 변환
    cv::Vec3f target_color_float(target_color[0], target_color[1], target_color[2]);
    cv::Scalar target_color_scalar(target_color_float[0], target_color_float[1], target_color_float[2]);

    // 각 픽셀과 타겟 색상 간의 차이 계산
    cv::Mat diff;
    cv::subtract(minimap_frame_float, target_color_scalar, diff);

    // 각 픽셀의 유클리드 거리 계산
    std::vector<cv::Mat> channels(3);
    cv::split(diff, channels);

    cv::Mat distance;
    cv::sqrt(channels[0].mul(channels[0]) + channels[1].mul(channels[1]) + channels[2].mul(channels[2]), distance);

    // 디버깅 정보 출력 (원하시면 제거 가능)
    double minVal, maxVal, meanVal;
    cv::minMaxLoc(distance, &minVal, &maxVal);
    meanVal = cv::mean(distance)[0];
//    LOGD("거리 값 통계 - 최소: %lf, 최대: %lf, 평균: %lf", minVal, maxVal, meanVal);
//    std::cout << "거리 값 통계 - 최소: " << minVal << ", 최대: " << maxVal << ", 평균: " << meanVal << std::endl;

    // 임계값 이하의 픽셀로 마스크 생성
    cv::Mat mask = distance <= threshold;
    mask.convertTo(mask, CV_8U, 255);

    // 노이즈 제거 (필요에 따라 모폴로지 연산 조정 가능)
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(1, 1));
    cv::Mat mask_clean;
    cv::morphologyEx(mask, mask_clean, cv::MORPH_OPEN, kernel);

    // 연결된 구성 요소 분석
    cv::Mat labels, stats, centroids;
    int num_components = cv::connectedComponentsWithStats(mask_clean, labels, stats, centroids, 8, CV_32S);

    // 최소 면적 제한 설정
    int MIN_AREA = 1;

    // 클러스터 수집 (면적과 픽셀 좌표 함께 저장)
    std::vector<std::pair<int, std::vector<cv::Point>>> clusters;

    for (int i = 1; i < num_components; ++i) {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area >= MIN_AREA) {
            std::vector<cv::Point> cluster_pixels;
            for (int y = 0; y < labels.rows; ++y) {
                for (int x = 0; x < labels.cols; ++x) {
                    if (labels.at<int>(y, x) == i) {
                        cluster_pixels.emplace_back(x, y);
                    }
                }
            }
            clusters.push_back({area, cluster_pixels});
            int x = stats.at<int>(i, cv::CC_STAT_LEFT);
            int y = stats.at<int>(i, cv::CC_STAT_TOP);
            int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
            int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);
//            LOGD("클러스터 %d: 면적=%d, 좌표 범위=(%d, %d, %d, %d)", i, area, x, y, w, h);
        }
    }

    if (clusters.empty()) {
        std::cout << "유의미한 클러스터를 찾지 못했습니다." << std::endl;
        return {};
    }

    // 면적 기준으로 클러스터 정렬 (내림차순)
    std::sort(clusters.begin(), clusters.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    // 가장 큰 클러스터 선택
    auto selected_cluster = clusters.front().second;

    return selected_cluster;
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
    cv::Mat minmapMat;
    minimap_frame.copyTo(minmapMat);

    auto now = std::chrono::steady_clock::now();

    if(isCharacterDetectionActive) {
        cv::Vec3b target_color(68, 221, 255); // 찾고자 하는 색상

        std::vector<cv::Point> player_points = find_color(minmapMat, target_color, 50);
        if (!player_points.empty()) {
            // 클러스터의 첫 번째 픽셀을 기준 좌표로 사용
            cv::Point center = player_points[0];
        } else {
            if (now - lastDetectionTime > detectionTimeout) {
                systemNotify("플레이어 탐지 실패");
            }
        }
    }

    if (!minimap_frame.empty()) {
        cv::Vec3b target_color(255, 102, 221); // 찾고자 하는 색상

        std::vector<cv::Point> runeMatch = find_color(minmapMat, target_color, 50);
        if (!runeMatch.empty()) {
            systemNotify("룬 탐지");
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

cv::UMat CaptureThread::filterColor(const cv::UMat &img, const std::vector<std::pair<cv::Scalar, cv::Scalar>> &ranges) {
    cv::UMat hsv;
    cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);

    cv::UMat mask, tempMask;
    for (const auto &range : ranges) {
        cv::inRange(hsv, range.first, range.second, tempMask);
        if (mask.empty()) {
            mask = tempMask.clone();
        } else {
            cv::bitwise_or(mask, tempMask, mask);
        }
    }

    cv::UMat result = cv::UMat::zeros(img.size(), img.type());
    img.copyTo(result, mask);

    return result;
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
//        cv::cvtColor(jpegUMat, bgrUMat, cv::COLOR_BGR2RGB);
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

cv::Mat decodeBufferToMat(const std::vector<uchar>& buffer) {
    if (buffer.empty()) {
        return cv::Mat(); // buffer가 비어있으면 빈 Mat 반환
    }

    // buffer를 cv::Mat으로 디코딩
    cv::Mat decodedMat = cv::imdecode(buffer, cv::IMREAD_UNCHANGED);

    return decodedMat;
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_example_macro_capture_CaptureThread_nativeGetMinimapData(JNIEnv *env, jobject thiz,
                                                                  jlong native_obj) {
    CaptureThread *captureThread = reinterpret_cast<CaptureThread *>(native_obj);
    cv::UMat minimap = captureThread->getMinimap();

    if (minimap.empty()) {
        return nullptr; // Minimap이 비어있으면 null 반환
    }

    cv::Mat mat;
//    minimap.copyTo(mat);
    cv::cvtColor(minimap, mat, cv::COLOR_BGR2RGB);

    std::ostringstream jsonStream;
    jsonStream << "["; // JSON 배열 시작

    for (int i = 0; i < mat.rows; ++i) {
        jsonStream << "["; // Row 배열 시작
        for (int j = 0; j < mat.cols; ++j) {
            // RGB 포맷 픽셀 읽기
            const cv::Vec3b &pixel = mat.at<cv::Vec3b>(i, j);
            jsonStream << "[" << (int)pixel[0] << ", " << (int)pixel[1] << ", " << (int)pixel[2] << "]";
            if (j != mat.cols - 1) jsonStream << ", "; // Row 내 쉼표 처리
        }
        jsonStream << "]"; // Row 배열 끝
        if (i != mat.rows - 1) jsonStream << ", "; // Rows 간 쉼표 처리
    }

    jsonStream << "]"; // JSON 배열 끝

    // JSON 문자열 반환
    std::string jsonString = jsonStream.str();
    return env->NewStringUTF(jsonString.c_str());
}

// Base64 인코딩 함수
std::string base64_encode(const uchar* bytes_to_encode, size_t len) {
    static const std::string base64_chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

    std::string ret;
    int i = 0;
    int j = 0;
    uchar char_array_3[3];
    uchar char_array_4[4];

    while (len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    return ret;
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_example_macro_capture_CaptureThread_nativeGetMinimapImage(JNIEnv *env, jobject thiz,
                                                                   jlong native_obj) {
    // Convert native object to CaptureThread
    CaptureThread *captureThread = reinterpret_cast<CaptureThread *>(native_obj);

    // Get the minimap image
    cv::UMat minimap = captureThread->getMinimap();

    if (minimap.empty()) {
        return nullptr; // Minimap이 비어있으면 null 반환
    }

    // Encode the Mat image to a buffer
    std::vector<uchar> buffer;
    cv::imencode(".png", minimap, buffer);

    if (buffer.empty()) {
        __android_log_print(ANDROID_LOG_ERROR, "nativeGetMinimapImage", "Image encoding failed.");
        return nullptr;
    }
    __android_log_print(ANDROID_LOG_INFO, "nativeGetMinimapImage", "Encoded image size: %zu", buffer.size());

    // Create a Java byte array and copy the buffer data
    jbyteArray byteArray = env->NewByteArray(buffer.size());
    if (byteArray == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "nativeGetMinimapImage", "Failed to create jbyteArray.");
        return nullptr;
    }
    env->SetByteArrayRegion(byteArray, 0, buffer.size(), reinterpret_cast<jbyte *>(buffer.data()));

    return byteArray;
}
extern "C"
JNIEXPORT jstring JNICALL
Java_com_example_macro_capture_CaptureThread_nativeGetMinimapImageBase64(JNIEnv *env, jobject thiz,
                                                                         jlong native_obj) {
    // Convert native object to CaptureThread
    CaptureThread *captureThread = reinterpret_cast<CaptureThread *>(native_obj);

    // Get the minimap image
    cv::UMat minimap = captureThread->getMinimap();
    if (minimap.empty()) {
        __android_log_print(ANDROID_LOG_ERROR, "nativeGetMinimapImageBase64", "Minimap is empty.");
        return nullptr;
    }

    // Encode the Mat image to a buffer
    std::vector<uchar> buffer;
    if (!cv::imencode(".png", minimap, buffer)) {
        __android_log_print(ANDROID_LOG_ERROR, "nativeGetMinimapImageBase64", "Image encoding failed.");
        return nullptr;
    }

    // Convert the buffer to Base64
    std::string base64_string = base64_encode(buffer.data(), buffer.size());

    // Return the Base64 string as a Java string
    return env->NewStringUTF(base64_string.c_str());
}