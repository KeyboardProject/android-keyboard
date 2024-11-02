//
// Created by ccxz8 on 2024-08-29.
//

#ifndef MACRO_INPUTSERVICE_H
#define MACRO_INPUTSERVICE_H

#include "grpcpp/grpcpp.h"
#include "grpc/input_service.grpc.pb.h"
#include <android/log.h>
#include "GrpcServer.h"
#include "../jni_helper.h"
#include "../jvm/JvmManager.h"

#define INPUT_SERVICE_LOG_TAG "InputService"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, INPUT_SERVICE_LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, INPUT_SERVICE_LOG_TAG, __VA_ARGS__)

using MacroReplayCallback = std::function<void(const std::string&)>;

class InputService final : public GrpcServer, Input::Service {

    grpc::Status StartRecording(grpc::ServerContext* context, const StartRequest* request,
                                StatusResponse* response) override;
    grpc::Status StopRecording(grpc::ServerContext* context, const StopRequest* request,
                               StatusResponse* response) override;
    grpc::Status StartReplay(grpc::ServerContext* context, const ReplayRequest* request,
                             StatusResponse* response) override;
    grpc::Status ReplayMacroDebug(grpc::ServerContext* context, const ReplayRequest* request,
                                  grpc::ServerWriter<MacroEvent>* writer) override;
    grpc::Status ListSaveFiles(grpc::ServerContext* context, const ListRequest* request,
                               SaveFilesResponse* response) override;
    grpc::Status StopReplay(grpc::ServerContext* context, const StopReplayRequest* request,
                            StatusResponse* response) override;
    grpc::Status SaveMacro(grpc::ServerContext* context, const SaveMacroRequest* request,
                           SaveMacroResponse* response) override;
    grpc::Status GetMacroDetail(grpc::ServerContext* context, const GetMacroDetailRequest* request,
                                GetMacroDetailResponse* response) override;
    grpc::Status DeleteMacros(grpc::ServerContext* context, const DeleteMacrosRequest* request,
                              StatusResponse* response) override;
    grpc::Status StartComplexReplay(grpc::ServerContext* context, const ComplexReplayRequest* request,
                                    StatusResponse* response) override;

    grpc::Status ImportProfile(grpc::ServerContext* context, const ImportProfileRequest* request, StatusResponse* response) override;
    grpc::Status ExportProfile(grpc::ServerContext* context, const ExportProfileRequest* request, ExportProfileResponse* response) override;

    jobject createComplexReplayRequestObject(JNIEnv* env, const ComplexReplayRequest* request);
    jobject createReplayTaskObject(JNIEnv* env, const ReplayTask& task);
    jobject createReplayTaskList(JNIEnv* env, const google::protobuf::RepeatedPtrField<ReplayTask>& tasks);


    void startMacroReplay(const std::string& filename, MacroReplayCallback callback);
public:
    InputService(JNIEnv* env, jobject instance) {
        // Java 객체를 전역 참조로 저장
        this->kotlinInstance = env->NewGlobalRef(instance);
    }
    void init(int port);
};


#endif //MACRO_INPUTSERVICE_H
