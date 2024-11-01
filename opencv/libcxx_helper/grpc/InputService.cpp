//
// Created by ccxz8 on 2024-08-29.
//

#include "InputService.h"
#include <jni.h>

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

grpc::Status InputService::StartComplexReplay(grpc::ServerContext* context, const ComplexReplayRequest* request, StatusResponse* response) {

    std::cout<<"복잡한 요청 실행"<<request->repeatcount()<<'\n';

    std::cout << "실행 종료: "<< std::endl;
    return grpc::Status::OK;
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

grpc::Status InputService::ImportProfile(grpc::ServerContext* context, const ImportProfileRequest* request, StatusResponse* response) {

    return grpc::Status::OK;
}

grpc::Status InputService::ExportProfile(grpc::ServerContext* context, const ExportProfileRequest* request, ExportProfileResponse* response) {

    return grpc::Status::OK;
}

void InputService::init(int port) {
    initServer(port, this);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_example_macro_grpc_InputService_nativeCreateObject(JNIEnv *env, jobject thiz, jint port) {
    gJavaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    InputService *pInputService = new InputService(env, thiz);
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