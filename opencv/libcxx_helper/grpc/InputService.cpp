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