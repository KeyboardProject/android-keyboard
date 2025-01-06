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
#include "../CaptureThread.h"

#define INPUT_SERVICE_LOG_TAG "InputService"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, INPUT_SERVICE_LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, INPUT_SERVICE_LOG_TAG, __VA_ARGS__)

using MacroReplayCallback = std::function<void(const std::string&)>;

class InputService final : public GrpcServer, Input::Service, public CaptureSystemNotify {
    CaptureThread* captureThread;

    bool complexRepalyFlag;
    grpc::ServerWriter<StatusResponse>* complexReplayErrorWriter;

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
    grpc::Status StartComplexReplay(grpc::ServerContext* context, const ComplexReplayRequest* request, grpc::ServerWriter<StatusResponse>* writer) override;

    grpc::Status ImportProfile(grpc::ServerContext* context, const ImportProfileRequest* request, StatusResponse* response) override;
    grpc::Status ExportProfile(grpc::ServerContext* context, const ExportProfileRequest* request, ExportProfileResponse* response) override;

    jobject createImportProfileRequestObject(JNIEnv* env, const ImportProfileRequest* request);
    jobject createExportProfileRequestObject(JNIEnv* env, const ExportProfileRequest* request);

    grpc::Status RemoteKeyEvent(grpc::ServerContext* context, grpc::ServerReader<KeyboardEvent>* reader, InputEmpty* response) override;
    grpc::Status RemoteMouseEvent(grpc::ServerContext* context, grpc::ServerReader<MouseEvent>* reader, InputEmpty* response) override;

    jobject createComplexReplayRequestObject(JNIEnv* env, const ComplexReplayRequest* request);
    jobject createReplayTaskObject(JNIEnv* env, const ReplayTask& task);
    jobject createReplayTaskList(JNIEnv* env, const google::protobuf::RepeatedPtrField<ReplayTask>& tasks);

    void startMacroReplay(const std::string& filename, MacroReplayCallback callback);

    void onCaptureSystemNotify(std::string message) override;
public:
    InputService(JNIEnv* env, jobject instance, CaptureThread* pCaptureThread) {
        // Java 객체를 전역 참조로 저장
        this->captureThread = pCaptureThread;
        this->kotlinInstance = env->NewGlobalRef(instance);
        this->captureThread->addObserver(this);
        complexRepalyFlag = false;
    }
    void init(int port);
};

class CharacterCheckObserver {
public:
    virtual ~CharacterCheckObserver() = default;
    virtual void onCharacterCheckChanged(bool newStatus) = 0;
};


#endif //MACRO_INPUTSERVICE_H
