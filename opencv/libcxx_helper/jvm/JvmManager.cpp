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

void JvmManager::cleanupJValues(JNIEnv* env, const std::vector<JvmManager::JvalueWithType>& args) {
    for (const auto& arg : args) {
        if (arg.type == JavaArgType::STRING && arg.value.l != nullptr) {
            env->DeleteLocalRef(arg.value.l);
        }
    }
}

std::vector<JvmManager::JvalueWithType> JvmManager::convertToJValues(JNIEnv* env, const std::vector<JavaArg>& args) {
    std::vector<JvmManager::JvalueWithType> jvalues;
    for (const auto& arg : args) {
        JvalueWithType jArgWithType = {};
        if (std::holds_alternative<std::string>(arg)) {
            jArgWithType.value.l = env->NewStringUTF(std::get<std::string>(arg).c_str());
            jArgWithType.type = JavaArgType::STRING;
        } else if (std::holds_alternative<int>(arg)) {
            jArgWithType.value.i = std::get<int>(arg);
            jArgWithType.type = JavaArgType::INT;
        } else if (std::holds_alternative<long>(arg)) {
            jArgWithType.value.j = std::get<long>(arg);
            jArgWithType.type = JavaArgType::LONG;
        } else if (std::holds_alternative<bool>(arg)) {
            jArgWithType.value.z = static_cast<jboolean>(std::get<bool>(arg));
            jArgWithType.type = JavaArgType::BOOLEAN;
        }
        jvalues.push_back(jArgWithType);
    }
    return jvalues;
}

std::vector<jvalue> JvmManager::extractJvalues(const std::vector<JvmManager::JvalueWithType>& jvaluesWithType) {
    std::vector<jvalue> jvalues;
    for (const auto& jvalueWithType : jvaluesWithType) {
        jvalues.push_back(jvalueWithType.value);  // jvalueWithType의 jvalue 필드만 추출
    }
    return jvalues;
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
    std::vector<JvmManager::JvalueWithType> jvaluesWithType  = convertToJValues(env, args);
    std::vector<jvalue> jvalues = extractJvalues(jvaluesWithType);

    invokeJavaMethod(env, methodID, jvalues);  // 반환값 없는 메서드 호출

    cleanupJValues(env, jvaluesWithType);
    if (isAttached) gJavaVM->DetachCurrentThread();
}

void JvmManager::invokeJavaMethod(JNIEnv* env, jmethodID methodID, const std::vector<jvalue>& args) {
    if (args.empty()) {
        env->CallVoidMethod(kotlinInstance, methodID);  // void 메서드 호출
    } else {
        env->CallVoidMethodA(kotlinInstance, methodID, args.data());  // void 메서드 호출
    }
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