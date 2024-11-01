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
    enum class JavaArgType {
        STRING,
        INT,
        LONG,
        BOOLEAN,
        OTHER // 추가 가능
    };

    struct JvalueWithType {
        jvalue value;
        JavaArgType type;
    };
protected:
    jobject kotlinInstance;

    void callJavaMethod(const std::string& methodName, const std::string& methodSig,
                     const std::vector<JavaArg>& args);

    jmethodID findJavaMethod(JNIEnv* env, const std::string& methodName, const std::string& methodSig);

    JNIEnv* getJNIEnv(bool& isAttached);

    void cleanupJValues(JNIEnv* env, const std::vector<JvmManager::JvalueWithType>& args);

    void invokeJavaMethod(JNIEnv* env, jmethodID methodID, const std::vector<jvalue>& args);

    std::vector<JvmManager::JvalueWithType> convertToJValues(JNIEnv* env, const std::vector<JavaArg>& args);

    template <class T>
    T callJavaMethod(const std::string& methodName, const std::string& methodSig,
                     const std::vector<JavaArg>& args, const std::string& returnType, JobjectMapper<T> mapper){
        bool isAttached;
        JNIEnv* env = getJNIEnv(isAttached);
        if (!env) {
            LOGE("Failed to obtain JNIEnv");
            return T();
        }

        jmethodID methodID = findJavaMethod(env, methodName, methodSig);
        if (methodID == nullptr) {
            if (isAttached) {
                gJavaVM->DetachCurrentThread();
            }
            return T();
        }

        // 인자가 있을 경우 변환, 없으면 빈 벡터 전달
        std::vector<JvmManager::JvalueWithType> jvaluesWithType  = convertToJValues(env, args);
        std::vector<jvalue> jvalues = extractJvalues(jvaluesWithType);

        T result;

        result = invokeJavaMethodWithReturn<T>(env, methodID, jvalues, returnType, mapper);

        // 인자들에 대한 로컬 참조 해제
        cleanupJValues(env, jvaluesWithType);

        // 네이티브 스레드 해제
        if (isAttached) {
            gJavaVM->DetachCurrentThread();
        }

        return result;
    }

    std::vector<jvalue> extractJvalues(const std::vector<JvmManager::JvalueWithType>& jvaluesWithType);

    template <class T>
    T invokeJavaMethodWithReturn(JNIEnv* env, jmethodID methodID, const std::vector<jvalue>& args, const std::string& returnType, JobjectMapper<T> mapper){
        jobject result = args.empty()
                         ? env->CallObjectMethod(kotlinInstance, methodID)
                         : env->CallObjectMethodA(kotlinInstance, methodID, args.data());

        jclass expectedClass = env->FindClass(returnType.substr(1, returnType.size() - 2).c_str());

        if (!env->IsInstanceOf(result, expectedClass)) {
            LOGE("Type mismatch: expected %s, but got a different type.", returnType.c_str());
            return T();
        }

        if (!mapper) {
            LOGE("Mapper function required for object type");
            return T();
        }

        return mapper(env, result);
    }
public:
    static int mapInteger(JNIEnv* env, jobject integerObj);
    static bool mapBoolean(JNIEnv* env, jobject booleanObj);
    static std::string mapString(JNIEnv* env, jobject stringObj);
};


#endif //MACRO_JVMMANAGER_H
