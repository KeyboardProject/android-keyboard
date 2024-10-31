//
// Created by ccxz8 on 2024-10-31.
//

#ifndef MACRO_JVMMANAGER_H
#define MACRO_JVMMANAGER_H

#include <string>
#include <vector>
#include <variant>
#include <functional>
#include <jni.h>
#include <android/log.h>
#include "../jni_helper.h"

#define JVM_LOG_TAG "JvmManager"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, JVM_LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, JVM_LOG_TAG, __VA_ARGS__)

template <typename T>
using JobjectMapper = std::function<T(JNIEnv*, jobject)>;
using JavaArg = std::variant<std::string, int, long, bool>;


class JvmManager {
protected:
    jobject kotlinInstance;

    template <typename T>
    T callJavaMethod(const std::string& methodName, const std::string& methodSig,
                          const std::vector<JavaArg>& args, const std::string& returnType, JobjectMapper<T> mapper);

    void callJavaMethod(const std::string& methodName, const std::string& methodSig,
                     const std::vector<JavaArg>& args);

    jmethodID findJavaMethod(JNIEnv* env, const std::string& methodName, const std::string& methodSig);

    JNIEnv* getJNIEnv(bool& isAttached);

    std::vector<jvalue> convertToJValues(JNIEnv* env, const std::vector<std::string>& stringArgs);

    void cleanupJValues(JNIEnv* env, const std::vector<jvalue>& args);

    void invokeJavaMethod(JNIEnv* env, jmethodID methodID, const std::vector<jvalue>& args);

    std::vector<jvalue> convertToJValues(JNIEnv* env, const std::vector<JavaArg>& args);

    template <typename T>
    T invokeJavaMethodWithReturn(JNIEnv* env, jmethodID methodID, const std::vector<jvalue>& args, const std::string& returnType, JobjectMapper<T> mapper);
public:
    static int mapInteger(JNIEnv* env, jobject integerObj);
    static bool mapBoolean(JNIEnv* env, jobject booleanObj);
    static std::string mapString(JNIEnv* env, jobject stringObj);
};


#endif //MACRO_JVMMANAGER_H
