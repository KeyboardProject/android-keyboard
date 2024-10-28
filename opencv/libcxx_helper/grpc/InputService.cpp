//
// Created by ccxz8 on 2024-08-29.
//

#include "InputService.h"
#include <jni.h>

grpc::Status InputService::StartRecording(grpc::ServerContext* context, const StartRequest* request,
                                              StatusResponse* response) {
    LOGI("녹화 시작");
    std::cout << "녹화 시작, 파일 이름: " << request->filename() << std::endl;
    response->set_message("녹화 시작");
    return grpc::Status::OK;
}


grpc::Status InputService::StopRecording(grpc::ServerContext* context, const StopRequest* request,
                                             StatusResponse* response) {
    std::cout << "녹화 중지" << std::endl;
    // 로그 기록 중지

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

extern "C"
JNIEXPORT jlong JNICALL
Java_com_example_macro_grpc_InputService_nativeCreateObject(JNIEnv *env, jobject thiz) {
    InputService *pInputService = new InputService();
    return reinterpret_cast<jlong>(pInputService);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_macro_grpc_InputService_nativeDestroyObject(JNIEnv *env, jobject thiz,
                                                             jlong native_obj) {
    delete reinterpret_cast<InputService *>(native_obj);
}