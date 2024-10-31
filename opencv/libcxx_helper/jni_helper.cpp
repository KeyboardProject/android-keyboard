// jni_helper.cpp
#include "jni_helper.h"

JavaVM* gJavaVM = nullptr;  // JavaVM 포인터 초기화

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    gJavaVM = vm;  // 전역 JavaVM 포인터 설정
    return JNI_VERSION_1_6;
}