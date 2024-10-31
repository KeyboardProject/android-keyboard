//
// Created by ccxz8 on 2024-10-31.
//

#include "JvmManager.h"

JNIEnv* JvmManager::getJNIEnv(bool& isAttached) {
    JNIEnv* env;
    isAttached = false;

    if (gJavaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        gJavaVM->AttachCurrentThread(&env, nullptr);
        isAttached = true;
    }

    return env;
}

jmethodID JvmManager::findJavaMethod(JNIEnv* env, const std::string& methodName, const std::string& methodSig) {
    jclass clazz = env->GetObjectClass(kotlinInstance);
    jmethodID methodID = env->GetMethodID(clazz, methodName.c_str(), methodSig.c_str());
    if (!methodID) {
        LOGE("Failed to find method %s with signature %s", methodName.c_str(), methodSig.c_str());
    }
    return methodID;
}

std::vector<jvalue> JvmManager::convertToJValues(JNIEnv* env, const std::vector<std::string>& stringArgs) {
    std::vector<jvalue> args;
    for (const auto& arg : stringArgs) {
        jvalue jArg;
        jArg.l = env->NewStringUTF(arg.c_str());
        args.push_back(jArg);
    }
    return args;
}

void JvmManager::cleanupJValues(JNIEnv* env, const std::vector<jvalue>& args) {
    for (const auto& arg : args) {
        env->DeleteLocalRef(arg.l);
    }
}

std::vector<jvalue> JvmManager::convertToJValues(JNIEnv* env, const std::vector<JavaArg>& args) {
    std::vector<jvalue> jvalues;
    for (const auto& arg : args) {
        jvalue jArg;
        if (std::holds_alternative<std::string>(arg)) {
            jArg.l = env->NewStringUTF(std::get<std::string>(arg).c_str());
        } else if (std::holds_alternative<int>(arg)) {
            jArg.i = std::get<int>(arg);
        } else if (std::holds_alternative<long>(arg)) {
            jArg.j = std::get<long>(arg);
        } else if (std::holds_alternative<bool>(arg)) {
            jArg.z = std::get<bool>(arg);
        }
        jvalues.push_back(jArg);
    }
    return jvalues;
}

template <typename T>
T JvmManager::callJavaMethod(const std::string& methodName, const std::string& methodSig,
                                  const std::vector<JavaArg>& args, const std::string& returnType, JobjectMapper<T> mapper) {
    bool isAttached;
    JNIEnv* env = getJNIEnv(isAttached);
    if (!env) {
        LOGE("Failed to obtain JNIEnv");
        return;
    }

    jmethodID methodID = findJavaMethod(env, methodName, methodSig);
    if (methodID == nullptr) {
        if (isAttached) {
            gJavaVM->DetachCurrentThread();
        }
        return;
    }

    // 인자가 있을 경우 변환, 없으면 빈 벡터 전달
    std::vector<jvalue> jvalues = convertToJValues(env, args);

    T result;

    result = callJavaMethodWithReturn<T>(env, methodID, jvalues);

    // 인자들에 대한 로컬 참조 해제
    cleanupJValues(env, jvalues);

    // 네이티브 스레드 해제
    if (isAttached) {
        gJavaVM->DetachCurrentThread();
    }

    return result;
}

void JvmManager::callJavaMethod(const std::string& methodName, const std::string& methodSig,
                                      const std::vector<JavaArg>& args) {
    bool isAttached;
    JNIEnv* env = getJNIEnv(isAttached);
    if (!env) {
        LOGE("Failed to obtain JNIEnv");
        return;
    }

    jmethodID methodID = findJavaMethod(env, methodName, methodSig);
    if (methodID == nullptr) {
        if (isAttached) gJavaVM->DetachCurrentThread();
        return;
    }

    // 인자를 jvalue로 변환
    std::vector<jvalue> jvalues = convertToJValues(env, args);

    invokeJavaMethod(env, methodID, jvalues);  // 반환값 없는 메서드 호출

    cleanupJValues(env, jvalues);
    if (isAttached) gJavaVM->DetachCurrentThread();
}

void JvmManager::invokeJavaMethod(JNIEnv* env, jmethodID methodID, const std::vector<jvalue>& args) {
    if (args.empty()) {
        env->CallVoidMethod(kotlinInstance, methodID);  // void 메서드 호출
    } else {
        env->CallVoidMethodA(kotlinInstance, methodID, args.data());  // void 메서드 호출
    }
}

template <typename T>
T JvmManager::invokeJavaMethodWithReturn(JNIEnv* env, jmethodID methodID, const std::vector<jvalue>& args, const std::string& returnType, JobjectMapper<T> mapper) {
    jobject result = args.empty()
                     ? env->CallObjectMethod(kotlinInstance, methodID)
                     : env->CallObjectMethodA(kotlinInstance, methodID, args.data());

    jclass expectedClass = env->FindClass(returnType.substr(1, returnType.size() - 2).c_str());

    if (!env->IsInstanceOf(result, expectedClass)) {
        LOGE("Type mismatch: expected %s, but got a different type.", returnType.c_str());
        return T();
    }

    if (mapper) {
        LOGE("Mapper function required for object type");
        return T();
    }

    return mapper(env, result);
}

/* static method */

int JvmManager::mapInteger(JNIEnv* env, jobject integerObj) {
    jclass integerClass = env->FindClass("java/lang/Integer");
    jmethodID intValueMethod = env->GetMethodID(integerClass, "intValue", "()I");
    return env->CallIntMethod(integerObj, intValueMethod);
}

bool JvmManager::mapBoolean(JNIEnv* env, jobject booleanObj) {
    jclass booleanClass = env->FindClass("java/lang/Boolean");
    jmethodID booleanValueMethod = env->GetMethodID(booleanClass, "booleanValue", "()Z");
    return env->CallBooleanMethod(booleanObj, booleanValueMethod);
}

std::string JvmManager::mapString(JNIEnv* env, jobject stringObj) {
    const char* str = env->GetStringUTFChars(static_cast<jstring>(stringObj), nullptr);
    std::string cppString(str);
    env->ReleaseStringUTFChars(static_cast<jstring>(stringObj), str);
    return cppString;
}