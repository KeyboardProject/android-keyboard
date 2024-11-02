// jni_helper.cpp
#include "jni_helper.h"

JavaVM* gJavaVM = nullptr;  // JavaVM 포인터 초기화
static jobject gClassLoader;
static jmethodID gFindClassMethod;

JNIEnv* getEnv() {
    JNIEnv *env;
    int status = gJavaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
    if(status < 0) {
        status = gJavaVM->AttachCurrentThread(&env, NULL);
        if(status < 0) {
            return nullptr;
        }
    }
    return env;
}

jclass findClass(const char* name) {
    JNIEnv* env = getEnv();  // 현재 스레드의 JNIEnv 가져오기

    // 클래스 이름을 Java 문자열로 변환
    jstring className = env->NewStringUTF(name);

    // gClassLoader의 findClass 메서드를 호출하여 클래스를 찾음
    jclass clazz = static_cast<jclass>(env->CallObjectMethod(gClassLoader, gFindClassMethod, className));

    // 로컬 참조 해제
    env->DeleteLocalRef(className);

    return clazz;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    gJavaVM = vm;  // 전역 JavaVM 포인터 설정
    auto env = getEnv();

    auto randomClass = env->FindClass("com/example/macro/MainActivity");
    jclass classClass = env->GetObjectClass(randomClass);
    auto classLoaderClass = env->FindClass("java/lang/ClassLoader");
    jmethodID getClassLoaderMethod = env->GetMethodID(classClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject localClassLoader = env->CallObjectMethod(randomClass, getClassLoaderMethod);

    // 로컬 참조를 글로벌 참조로 변경
    gClassLoader = env->NewGlobalRef(localClassLoader);
    gFindClassMethod = env->GetMethodID(classLoaderClass, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");

    // 로컬 참조 해제
    env->DeleteLocalRef(localClassLoader);
    env->DeleteLocalRef(randomClass);
    env->DeleteLocalRef(classClass);
    env->DeleteLocalRef(classLoaderClass);
    return JNI_VERSION_1_6;
}