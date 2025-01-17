//
// Created by ccxz8 on 2025-01-16.
//

#include "CubeService.h"

cv::UMat CubeService::readImageFromAssets(AAssetManager* mgr, const char* filename) {
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

CubeService::CubeService(JNIEnv* env, jobject javaInstance, AAssetManager* mgr)
    : mgr(mgr)  {
    kotlinInstance = javaInstance;  // JvmManager의 kotlinInstance 설정
    frame_empty = true;
    previous_frame_empty = true;
    CUBE_TL_TEMPLATE = readImageFromAssets(mgr, "cube_tl.png");
    CUBE_BR_TEMPLATE = readImageFromAssets(mgr, "cube_br.png");
}

CubeService::~CubeService() {

}

void CubeService::onFrameCapture(const cv::UMat& frame) {
    std::lock_guard<std::mutex> lock(mutex);
    
    // 큐브 영역 검출
    cv::UMat cube_region = detectCubeRegion(frame);
    if (!cube_region.empty()) {
        processFrame(cube_region);
    }
}

void CubeService::processFrame(const cv::UMat& frame) {
    frame_empty = frame.empty();
    
    if (!frame_empty) {
        last_frame = frame.clone();
        cv::Mat mat;
        frame.copyTo(mat);
        last_ocr_text = performOcr(mat);
    }
    
    previous_frame_empty = frame_empty;
}

std::string CubeService::performOcr(const cv::Mat& mat) {
    // Mat을 바이트 버퍼로 변환
    std::vector<uchar> buffer;
    cv::imencode(".png", mat, buffer);
    
    // Java 메서드 호출을 위한 인자 준비
    std::vector<JavaArg> args = {
        static_cast<int>(mat.cols),  // width
        static_cast<int>(mat.rows),  // height
        jobject(buffer.data())       // byteBuffer
    };
    
    // Java의 recognizeText 메서드 호출
    return callJavaMethod<std::string>(
        "recognizeText", 
        "(IILjava/nio/ByteBuffer;)Ljava/lang/String;",
        args,
        "Ljava/lang/String;",
        JvmManager::mapString
    );
}

cv::UMat CubeService::getLastProcessedFrame() const {
    std::lock_guard<std::mutex> lock(mutex);
    return last_frame;
}

std::string CubeService::getLastOcrText() const {
    std::lock_guard<std::mutex> lock(mutex);
    return last_ocr_text;
}

bool CubeService::isRerolled() const {
    std::lock_guard<std::mutex> lock(mutex);
    return previous_frame_empty && !frame_empty;
}

cv::UMat CubeService::detectCubeRegion(const cv::UMat& frame) {
    if (!CUBE_TL_TEMPLATE.empty() && !CUBE_BR_TEMPLATE.empty()) {
        auto tl_match = single_match(frame, CUBE_TL_TEMPLATE);
        auto br_match = single_match(frame, CUBE_BR_TEMPLATE);
        
        if (tl_match.first.x >= 0 && br_match.first.x >= 0) {
            cube_tl = tl_match.first;
            cube_br = br_match.first;
            
            cv::Rect roi(cube_tl.x, cube_tl.y, 
                        cube_br.x - cube_tl.x + CUBE_BR_TEMPLATE.cols,
                        cube_br.y - cube_tl.y + CUBE_BR_TEMPLATE.rows);
            
            if (roi.x >= 0 && roi.y >= 0 && 
                roi.x + roi.width <= frame.cols && 
                roi.y + roi.height <= frame.rows) {
                return frame(roi);
            }
        }
    }
    return cv::UMat();
}

std::pair<cv::Point, cv::Point> CubeService::single_match(const cv::UMat& image, const cv::UMat& templ) {
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

extern "C"
JNIEXPORT jlong JNICALL
Java_com_example_macro_service_CubeService_nativeCreateObject(JNIEnv *env, jobject thiz,
                                                              jobject asset_manager,
                                                              jlong capture_thread_ptr) {
    JNIHepler::gJavaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    CaptureThread* pCaptureThread = reinterpret_cast<CaptureThread *>(capture_thread_ptr);
    AAssetManager *mgr = AAssetManager_fromJava(env, asset_manager);
    CubeService *pCubeService = new CubeService(env, thiz, mgr);
    pCaptureThread->addFrameObserver(pCubeService);
    return reinterpret_cast<jlong>(pCubeService);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_macro_service_CubeService_nativeDestroyObject(JNIEnv *env, jobject thiz,
                                                               jlong native_obj) {
    delete reinterpret_cast<CubeService *>(native_obj);
}