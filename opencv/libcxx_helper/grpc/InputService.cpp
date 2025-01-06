//
// Created by ccxz8 on 2024-08-29.
//

#include "InputService.h"
#include "../jni_helper.h"
#include <jni.h>
#include <iostream>
#include <string>
#include <vector>

grpc::Status InputService::StartRecording(grpc::ServerContext* context, const StartRequest* request,
                                              StatusResponse* response) {
    std::string str = "녹화 시작, 파일 이름: " + request->filename();
    LOGI("%s", str.c_str());

    std::string methodName = "startRequest";
    std::string methodSig = "(Ljava/lang/String;)V";
    std::vector<JavaArg> args = { request->filename() };

    callJavaMethod(methodName, methodSig, args);

    response->set_message("녹화 시작");
    return grpc::Status::OK;
}


grpc::Status InputService::StopRecording(grpc::ServerContext* context, const StopRequest* request,
                                             StatusResponse* response) {
    std::string str = "녹화 중지";
    LOGI("%s", str.c_str());

    std::string methodName = "stopRequest";
    std::string methodSig = "()V";
    std::vector<JavaArg> args;

    callJavaMethod(methodName, methodSig, args);

    response->set_message("녹화 중지");
    return grpc::Status::OK;
}

grpc::Status InputService::StartReplay(grpc::ServerContext* context, const ReplayRequest* request,
                                           StatusResponse* response) {
    std::cout << "매크로 재생 시작: " << request->filename() << std::endl;

    response->set_message("매크로 재생 시작");
    return grpc::Status::OK;
}

grpc::Status InputService::ReplayMacroDebug(grpc::ServerContext* context, const ReplayRequest* request,
                                                grpc::ServerWriter<MacroEvent>* writer) {
    std::cout << "매크로 재생 시작: " << request->filename() << " 디버그 모드" << std::endl;

    std::string methodName = "startReplayDebug";
    std::string methodSig = "(JLjava/lang/String;)V";
    std::vector<JavaArg> args = {
            reinterpret_cast<long>(writer), // writer 객체
            request->filename()             // file 이름
    };

    callJavaMethod(methodName, methodSig, args);

    std::cout << "실행 종료: " << request->filename() << " 디버그 모드" << std::endl;

    return grpc::Status::OK;
}

grpc::Status InputService::SaveMacro(grpc::ServerContext* context, const SaveMacroRequest* request, SaveMacroResponse* response) {

    return grpc::Status::OK;
}

grpc::Status InputService::ListSaveFiles(grpc::ServerContext* context, const ListRequest* request,
                                             SaveFilesResponse* response) {
    std::string methodName = "getFileList";
    std::string methodSig = "()Ljava/util/List;";
    std::vector<JavaArg> args;

    JobjectMapper<std::vector<std::string>> fileListMapper = [](JNIEnv* env, jobject javaFileList) -> std::vector<std::string> {
        std::vector<std::string> fileList;

        // javaKeyEventList가 실제 Java List 객체인지 확인
        jclass listClass = env->FindClass("java/util/List");
        jmethodID sizeMethod = env->GetMethodID(listClass, "size", "()I");
        jmethodID getMethod = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

        jint listSize = env->CallIntMethod(javaFileList, sizeMethod);

        // Java List의 각 요소를 가져와서 C++ std::string으로 변환
        for (jint i = 0; i < listSize; ++i) {
            jobject javaString = env->CallObjectMethod(javaFileList, getMethod, i);
            const char* cStr = env->GetStringUTFChars(static_cast<jstring>(javaString), nullptr);
            fileList.push_back(cStr);
            env->ReleaseStringUTFChars(static_cast<jstring>(javaString), cStr);
            env->DeleteLocalRef(javaString); // 로컬 참조 해제
        }

        env->DeleteLocalRef(listClass); // 로컬 참조 해제
        return fileList;
    };

    // callJavaMethod 호출로 Java 메서드 getFileList를 호출하여 결과를 가져옵니다.
    std::vector<std::string> fileList = callJavaMethod<std::vector<std::string>>(methodName, methodSig, args, "Ljava/util/List;", fileListMapper);

    // 결과를 gRPC 응답 메시지로 설정
    for (const auto& fileName : fileList) {
        response->add_filenames(fileName);
    }

    return grpc::Status::OK;
}

grpc::Status InputService::StopReplay(grpc::ServerContext* context, const StopReplayRequest* request,
                                          StatusResponse* response) {
    std::cout<<"종료 요청\n";

    response->set_message("매크로 재생 중단");
    return grpc::Status::OK;
}

grpc::Status InputService::StartComplexReplay(grpc::ServerContext* context, const ComplexReplayRequest* request, grpc::ServerWriter<StatusResponse>* writer) {
    std::cout << "복잡한 요청 실행, 반복 횟수: " << request->repeatcount() << '\n';

    // 미니맵 캡쳐 시작
    if (captureThread) {
        captureThread->initCaptureMinimap();
    }

    // ComplexReplayRequest를 Kotlin 데이터 클래스에 맞게 변환
    std::string methodName = "startComplexReplay";
    std::string methodSig = "(Lcom/example/macro/macro/ComplexReplayRequest;)V";

    bool isAttached;
    JNIEnv* env = getJNIEnv(isAttached);  // JNIEnv 가져오기 함수 추가 필요

    if (env == nullptr) {
        std::cerr << "JNIEnv를 얻지 못했습니다." << std::endl;
        return grpc::Status(grpc::StatusCode::INTERNAL, "JNI 환경을 가져오지 못했습니다.");
    }

    // ComplexReplayRequest를 나타내는 Java 객체 생성
    jobject complexReplayRequestObj = createComplexReplayRequestObject(env, request);

    // Java 메서드 호출
    std::vector<JavaArg> args = {
            complexReplayRequestObj
    };

    this->complexRepalyFlag = true;
    this->complexReplayErrorWriter = writer;

    callJavaMethod(methodName, methodSig, args);

    this->captureThread->stopMinimap();
    this->complexRepalyFlag = false;

    std::cout << "복잡한 요청 실행 종료" << std::endl;
    return grpc::Status::OK;
}

// ComplexReplayRequest를 Java 객체로 변환하는 함수
jobject InputService::createComplexReplayRequestObject(JNIEnv* env, const ComplexReplayRequest* request) {
    // ComplexReplayRequest 클래스와 생성자 ID 가져오기
    jclass complexReplayRequestClass = JNIHepler::findClass("com/example/macro/macro/ComplexReplayRequest");
    jmethodID complexReplayRequestConstructor = env->GetMethodID(complexReplayRequestClass, "<init>", "(Ljava/util/List;I)V");

    // List<ReplayTask> 변환
    jobjectArray tasksArray = static_cast<jobjectArray>(createReplayTaskList(env,
                                                                             request->tasks()));

    // repeatCount를 전달
    jint repeatCount = request->repeatcount();

    // ComplexReplayRequest 객체 생성
    jobject complexReplayRequestObj = env->NewObject(complexReplayRequestClass, complexReplayRequestConstructor, tasksArray, repeatCount);
    return complexReplayRequestObj;
}

// ReplayTask 객체 리스트를 Java List로 변환하는 함수
jobject InputService::createReplayTaskList(JNIEnv* env, const google::protobuf::RepeatedPtrField<ReplayTask>& tasks) {
    // ArrayList 클래스 및 생성자 가져오기
    jclass arrayListClass = env->FindClass("java/util/ArrayList");;
    jmethodID arrayListConstructor = env->GetMethodID(arrayListClass, "<init>", "()V");
    jobject arrayListObj = env->NewObject(arrayListClass, arrayListConstructor);

    // ArrayList의 add 메서드 ID 가져오기
    jmethodID addMethod = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

    // ReplayTask 객체들을 ArrayList에 추가
    for (const auto& task : tasks) {
        jobject replayTaskObj = createReplayTaskObject(env, task);
        env->CallBooleanMethod(arrayListObj, addMethod, replayTaskObj);
        env->DeleteLocalRef(replayTaskObj);
    }
    return arrayListObj;
}

// 단일 ReplayTask 객체를 Java 객체로 변환하는 함수
jobject InputService::createReplayTaskObject(JNIEnv* env, const ReplayTask& task) {
    // ReplayTask 클래스 및 생성자 ID 가져오기
    jclass replayTaskClass = JNIHepler::findClass("com/example/macro/macro/ReplayTask");
    jmethodID replayTaskConstructor = env->GetMethodID(replayTaskClass, "<init>", "(Ljava/lang/String;II)V");

    // ReplayTask의 필드 값들 변환
    jstring filename = env->NewStringUTF(task.filename().c_str());
    jint delayAfter = task.delayafter();
    jint repeatCount = task.repeatcount();

    // ReplayTask 객체 생성
    jobject replayTaskObj = env->NewObject(replayTaskClass, replayTaskConstructor, filename, delayAfter, repeatCount);

    // Local 참조 해제
    env->DeleteLocalRef(filename);

    return replayTaskObj;
}


grpc::Status InputService::GetMacroDetail(grpc::ServerContext* context, const GetMacroDetailRequest* request, GetMacroDetailResponse* response) {

    std::string methodName = "getMacroDetail";
    std::string methodSig = "(Ljava/lang/String;)Ljava/util/List;";
    std::vector<JavaArg> args = { request->filename() };

    // 매퍼 함수: Java List<KeyEvent> -> C++ std::vector<KeyEvent>
    JobjectMapper<std::vector<KeyEvent>> keyEventMapper = [](JNIEnv* env, jobject javaKeyEventList) -> std::vector<KeyEvent> {
        std::vector<KeyEvent> cppEvents;

        // Java List<KeyEvent>에서 `size()` 메서드 호출
        jclass listClass = env->GetObjectClass(javaKeyEventList);
        jmethodID sizeMethod = env->GetMethodID(listClass, "size", "()I");
        jint size = env->CallIntMethod(javaKeyEventList, sizeMethod);

        // Java List<KeyEvent>에서 `get(int index)` 메서드 호출
        jmethodID getMethod = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

        // 리스트가 비어 있지 않은 경우 요소 타입 확인 및 추출
        if (size > 0) {
            jobject firstElement = env->CallObjectMethod(javaKeyEventList, getMethod, 0);

            if (firstElement != nullptr) {
                jclass elementClass = env->GetObjectClass(firstElement);
                jmethodID getClassMethod = env->GetMethodID(elementClass, "getClass", "()Ljava/lang/Class;");
                jobject elementClassObject = env->CallObjectMethod(firstElement, getClassMethod);

                // Class 객체에서 getName() 메서드를 호출하여 요소의 클래스 이름 확인
                jclass classClass = env->GetObjectClass(elementClassObject);
                jmethodID getNameMethod = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");
                jstring className = (jstring)env->CallObjectMethod(elementClassObject, getNameMethod);

                const char* classNameStr = env->GetStringUTFChars(className, nullptr);

                // 클래스 이름이 "com.example.macro.macro.KeyEvent"와 일치하는지 확인
                if (std::string(classNameStr) == "com.example.macro.macro.KeyEvent") {
                    // KeyEvent 클래스에서 `delay`와 `data` 필드 가져오기
                    jfieldID delayField = env->GetFieldID(elementClass, "delay", "J");
                    jfieldID dataField = env->GetFieldID(elementClass, "data", "[B");

                    // 각 요소를 C++ KeyEvent로 변환
                    for (jint i = 0; i < size; i++) {
                        jobject javaKeyEvent = env->CallObjectMethod(javaKeyEventList, getMethod, i);

                        // delay 필드 읽기
                        jlong delay = env->GetLongField(javaKeyEvent, delayField);

                        // data 필드를 byte array로 변환
                        jbyteArray javaDataArray = (jbyteArray)env->GetObjectField(javaKeyEvent, dataField);
                        jsize dataSize = env->GetArrayLength(javaDataArray);
                        std::vector<uint8_t> data(dataSize);
                        env->GetByteArrayRegion(javaDataArray, 0, dataSize, reinterpret_cast<jbyte*>(data.data()));

                        // KeyEvent 객체로 변환하여 벡터에 추가
                        KeyEvent event;
                        event.set_delay(static_cast<uint64_t>(delay));
                        std::string dataString(data.begin(), data.end());
                        event.set_data(dataString.c_str(), dataString.size());
                        cppEvents.push_back(event);

                        // 로컬 참조 해제
                        env->DeleteLocalRef(javaKeyEvent);
                        env->DeleteLocalRef(javaDataArray);
                    }
                }

                // JNI 리소스 해제
                env->ReleaseStringUTFChars(className, classNameStr);
                env->DeleteLocalRef(elementClassObject);
                env->DeleteLocalRef(className);
                env->DeleteLocalRef(classClass);
            }
        }

    // 최종 cppEvents 벡터 반환
        return cppEvents;
    };

    // Java 메서드 호출 및 반환된 데이터를 매퍼를 통해 C++ 벡터로 변환
    std::vector<KeyEvent> events = callJavaMethod<std::vector<KeyEvent>>(
            methodName, methodSig, args, "Ljava/util/List;", keyEventMapper);

    // response에 각 KeyEvent를 추가
    for (const auto& event : events) {
        auto* keyEvent = response->add_events();
        keyEvent->set_delay(event.delay());
        keyEvent->set_data(event.data().data(), event.data().size());
    }

    return grpc::Status::OK;
}

grpc::Status InputService::DeleteMacros(grpc::ServerContext* context, const DeleteMacrosRequest* request, StatusResponse* response) {

    response->set_message("파일 삭제 완료");
    return grpc::Status::OK;
}

// gRPC handler for ImportProfile
grpc::Status InputService::ImportProfile(grpc::ServerContext* context, 
                                       const ImportProfileRequest* request,
                                       StatusResponse* response) {
    std::string methodName = "importProfile";
    std::string methodSig = "(Ljava/lang/String;[B)V";
  
    bool isAttached;
    JNIEnv* env = getJNIEnv(isAttached);

    if (env == nullptr) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "Unable to obtain JNIEnv.");
    }

    jstring filename = nullptr;
    jbyteArray byteArray = nullptr;

    try {
        // 파일 이름 문자열 생성
        filename = env->NewStringUTF(request->filename().c_str());

        // 바이트 배열 생성
        const std::string& savData = request->savfile();
        byteArray = env->NewByteArray(savData.size());
        env->SetByteArrayRegion(byteArray, 0, savData.size(),
                              reinterpret_cast<const jbyte*>(savData.data()));

        // Java 메서드 호출
        std::vector<JavaArg> args = { JavaArg{filename}, JavaArg{byteArray} };
        callJavaMethod(methodName, methodSig, args);

        response->set_message("프로필 가져오기 성공: " + request->filename());
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        // 에러 발생 시 로컬 참조 정리
        if (filename != nullptr) env->DeleteLocalRef(filename);
        if (byteArray != nullptr) env->DeleteLocalRef(byteArray);

        return grpc::Status(grpc::StatusCode::INTERNAL, 
                          std::string("프로필 가져오기 실패: ") + e.what());
    }
}

// gRPC handler for ExportProfile
grpc::Status InputService::ExportProfile(grpc::ServerContext* context,
                                       const ExportProfileRequest* request,
                                       ExportProfileResponse* response) {
    std::string methodName = "exportProfile";
    std::string methodSig = "(Ljava/lang/String;)Ljava/util/List;";

    bool isAttached;
    JNIEnv* env = getJNIEnv(isAttached);

    if (env == nullptr) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "Unable to obtain JNIEnv.");
    }

    try {
        // 파라미터 준비
        jstring filename = env->NewStringUTF(request->filename().c_str());
        std::vector<JavaArg> args = { JavaArg{filename} };

        // Java 메서드 호출 및 KeyEvent 리스트 반환값 처리
        auto keyEventListMapper = [](JNIEnv* env, jobject list) -> std::vector<unsigned char> {
            std::vector<unsigned char> result;

            // List의 크기 가져오기
            jclass listClass = env->GetObjectClass(list);
            jmethodID sizeMethod = env->GetMethodID(listClass, "size", "()I");
            jint listSize = env->CallIntMethod(list, sizeMethod);

            // get 메서드 가져오기
            jmethodID getMethod = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

            // 모든 KeyEvent 처리
            for (jint i = 0; i < listSize; i++) {
                jobject keyEvent = env->CallObjectMethod(list, getMethod, i);
                
                // KeyEvent에서 data와 delay 가져오기
                jclass keyEventClass = env->GetObjectClass(keyEvent);
                jmethodID getDataMethod = env->GetMethodID(keyEventClass, "getData", "()[B");
                jmethodID getDelayMethod = env->GetMethodID(keyEventClass, "getDelay", "()J");
                
                // delay 값 가져오기
                jlong delay = env->CallLongMethod(keyEvent, getDelayMethod);
                
                // data 바이트 배열 가져오기
                jbyteArray byteArray = static_cast<jbyteArray>(env->CallObjectMethod(keyEvent, getDataMethod));
                jsize length = env->GetArrayLength(byteArray);
                jbyte* bytes = env->GetByteArrayElements(byteArray, nullptr);

                // 텍스트 형식으로 변환
                std::stringstream ss;
                ss << "Time diff: " << std::hex << std::uppercase << delay << "ns, Data: ";
                for (jsize j = 0; j < length; j++) {
                    if (j > 0) ss << " ";
                    ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') 
                       << (static_cast<int>(bytes[j]) & 0xFF);
                }
                ss << "\n";
                std::string line = ss.str();

                // 텍스트 라인을 바이트 배열로 변환하여 결과에 추가
                result.insert(result.end(), line.begin(), line.end());

                // 리소스 해제
                env->ReleaseByteArrayElements(byteArray, bytes, JNI_ABORT);
                env->DeleteLocalRef(byteArray);
                env->DeleteLocalRef(keyEvent);
                env->DeleteLocalRef(keyEventClass);
            }

            env->DeleteLocalRef(listClass);
            return result;
        };

        std::vector<unsigned char> fileData = callJavaMethod<std::vector<unsigned char>>(
            methodName, methodSig, args,
            "Ljava/util/List;",
            keyEventListMapper
        );

        // 응답 설정
        response->set_savfile(fileData.data(), fileData.size());
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, 
                          std::string("프로필 내보내기 실패: ") + e.what());
    }
}

grpc::Status InputService::RemoteKeyEvent(grpc::ServerContext* context, 
                                          grpc::ServerReader<KeyboardEvent>* reader, 
                                          InputEmpty* response) {
    KeyboardEvent event;
    while (reader->Read(&event)) {
        // KeyboardEvent 처리
        // event.data()를 사용하여 HID 리포트 데이터 처리

        // Java 메서드 호출 예시
        std::string methodName = "handleRemoteKeyEvent";
        std::string methodSig = "([B)V";
        std::vector<uint8_t> data(event.data().begin(), event.data().end());

        // LOGD("INPUT MESSAGE");

        bool isAttached;
        JNIEnv* env = getJNIEnv(isAttached);
        if (!env) {
            LOGE("Failed to get JNIEnv for RemoteKeyEvent");
            continue;
        }

        // jbyteArray 생성
        jbyteArray jData = env->NewByteArray(data.size());
        if (jData == nullptr) {
            LOGE("Failed to create jbyteArray for RemoteKeyEvent");
            if (isAttached) {
                JNIHepler::gJavaVM->DetachCurrentThread();
            }
            continue;
        }

        // jbyteArray에 데이터 설정
        env->SetByteArrayRegion(jData, 0, data.size(), reinterpret_cast<jbyte*>(data.data()));

        // JavaArg에 jbyteArray 추가
        std::vector<JavaArg> args = { static_cast<jobject>(jData) };

        // Java 메서드 호출
        callJavaMethod(methodName, methodSig, args);

        // 로컬 참조 해제 및 스레드 분리
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe(); // 예외 내용을 출력
            env->ExceptionClear();    // 예외 상태를 클리어
            LOGE("Exception occurred during SetByteArrayRegion");
            env->DeleteLocalRef(jData); // 이 경우 안전하게 삭제
            continue;
        }

        // 스레드 분리
        if (isAttached) {
            JNIHepler::gJavaVM->DetachCurrentThread();
        }
    }

    // 스트림이 종료되면 OK 반환
    return grpc::Status::OK;
}

grpc::Status InputService::RemoteMouseEvent(grpc::ServerContext* context, grpc::ServerReader<MouseEvent>* reader, InputEmpty* response) {
    MouseEvent event;
    while (reader->Read(&event)) {
        // MouseEvent 처리
        int32_t absoluteX = event.absolutex();
        int32_t absoluteY = event.absolutey();
        int32_t wheelDelta = event.wheeldelta();
        bool leftButton = event.leftbutton();
        bool rightButton = event.rightbutton();
        bool middleButton = event.middlebutton();

        // Java 메서드 호출 예시
        std::string methodName = "handleRemoteMouseEvent";
        std::string methodSig = "(IIIZZZ)V";
        std::vector<JavaArg> args = {
                absoluteX,
                absoluteY,
                wheelDelta,
                leftButton,
                rightButton,
                middleButton
        };

        callJavaMethod(methodName, methodSig, args);
    }

    // 스트림이 종료되면 OK 반환
    return grpc::Status::OK;
}

void InputService::init(int port) {
    initServer(port, this);
}

void InputService::onCaptureSystemNotify(std::string message) {
    LOGI("capture thread 상태가 변경되었습니다: %s", message.c_str());
    if (complexRepalyFlag && complexReplayErrorWriter != nullptr) {
        StatusResponse response;
        response.set_message(message);
        complexReplayErrorWriter->Write(response);
    }
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_example_macro_grpc_InputService_nativeCreateObject(JNIEnv *env, jobject thiz, jint port, jlong captureThreadPtr) {
    JNIHepler::gJavaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    CaptureThread* pCaptureThread = reinterpret_cast<CaptureThread *>(captureThreadPtr);
    InputService *pInputService = new InputService(env, thiz, pCaptureThread);
    pInputService->init(port);
    return reinterpret_cast<jlong>(pInputService);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_macro_grpc_InputService_nativeDestroyObject(JNIEnv *env, jobject thiz,
                                                             jlong native_obj) {
    delete reinterpret_cast<InputService *>(native_obj);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_macro_grpc_InputService_sendReplayKeyEvent(JNIEnv *env, jobject thiz,
                                                            jlong writer_ptr,
                                                            jstring event_description) {
    auto* writer = reinterpret_cast<grpc::ServerWriter<MacroEvent>*>(writer_ptr);

    // jstring을 C++ std::string으로 변환
    const char* eventDescriptionCStr = env->GetStringUTFChars(event_description, nullptr);
    std::string eventDescription(eventDescriptionCStr);
    env->ReleaseStringUTFChars(event_description, eventDescriptionCStr);

    // MacroEvent 객체에 설정하고 writer로 전송
    MacroEvent event;
    event.set_eventdescription(eventDescription);
    writer->Write(event);
}
