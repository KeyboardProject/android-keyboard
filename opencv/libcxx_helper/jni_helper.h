#ifndef JNI_HELPER_H
#define JNI_HELPER_H

#include <jni.h>
namespace JNIHepler {
    // 전역 JavaVM 포인터 선언
    extern JavaVM* gJavaVM;
    jclass findClass(const char* name);
    JNIEnv* getEnv();
}


#endif // JNI_HELPER_H