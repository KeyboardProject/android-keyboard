cmake_minimum_required(VERSION 3.10.1)

project(native_lib)

include(ExternalProject)

# 프로젝트 경로 설정
set(pathPROJECT ${CMAKE_SOURCE_DIR})
get_filename_component(pathPROJECT_PARENT ${pathPROJECT} DIRECTORY)
set(pathOPENCV ${pathPROJECT_PARENT})
set(libPath ${pathOPENCV}/native/libs/${ANDROID_ABI})
set(pathLIBOPENCV_JAVA ${pathOPENCV}/native/libs/${ANDROID_ABI}/libopencv_java4.so)
set(GRPC_PROTO_RELATIVE_PATH "${CMAKE_SOURCE_DIR}/../generated")

# gRPC 소스 디렉토리 설정
set(GRPC_SRC_DIR ${CMAKE_SOURCE_DIR}/../grpc)

# 타겟 빌드 디렉토리 설정
set(GRPC_BUILD_DIR ${CMAKE_SOURCE_DIR}/../outputs/${ANDROID_ABI})
file(MAKE_DIRECTORY ${GRPC_BUILD_DIR})

# gRPC 추가 (타겟 플랫폼용)
add_subdirectory(${GRPC_SRC_DIR} ${GRPC_BUILD_DIR})

get_filename_component(GRPC_PROTO_DIR ${GRPC_PROTO_RELATIVE_PATH} ABSOLUTE)

# 호스트 툴 실행 파일 경로 설정
set(PROTOBUF_PROTOC_EXECUTABLE ${pathOPENCV}/tools/protoc.exe)
set(GRPC_CPP_PLUGIN_EXECUTABLE ${pathOPENCV}/tools/grpc_cpp_plugin.exe)

message(${PROTOBUF_PROTOC_EXECUTABLE})

# 프로토콜 버퍼 파일 생성 디렉토리 설정
set(GRPC_PROTO_GENS_DIR "${pathOPENCV}/generated")
set(GRPC_PROTO_GRPC_GENS_DIR "${GRPC_PROTO_GENS_DIR}/grpc")
file(MAKE_DIRECTORY ${GRPC_PROTO_GRPC_GENS_DIR})

# 프로토콜 버퍼 파일들 설정
set(PROTO_BASE_DIR "${pathOPENCV}/proto")
file(GLOB_RECURSE PROTO_FILES "${PROTO_BASE_DIR}/*.proto")
if(NOT PROTO_FILES)
    message(SEND_ERROR "Error: No proto files found in ${PROTO_BASE_DIR}")
    return()
endif()

# 프로토콜 버퍼 파일 처리
set(PROTO_SRCS "")
set(PROTO_HDRS "")

include_directories(${pathOPENCV}/native/jni/include)
include_directories(includes)
include_directories(${ANDROID_NDK}/sources/android/cpufeatures)  # 추가
include_directories(${ANDROID_NDK}/sources/android/native_app_glue)  # 추가
include_directories(${GRPC_THREAD_DIR})
include_directories(${GRPC_PROTO_DIR})
include_directories(${GRPC_INCLUDE_PATH})

foreach(FIL ${PROTO_FILES})
    get_filename_component(FIL_WE ${FIL} NAME_WE)

    add_custom_command(
            OUTPUT "${GRPC_PROTO_GRPC_GENS_DIR}/${FIL_WE}.grpc.pb.cc"
            "${GRPC_PROTO_GRPC_GENS_DIR}/${FIL_WE}.grpc.pb.h"
            "${GRPC_PROTO_GRPC_GENS_DIR}/${FIL_WE}.pb.cc"
            "${GRPC_PROTO_GRPC_GENS_DIR}/${FIL_WE}.pb.h"
            COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
            ARGS --grpc_out=${GRPC_PROTO_GRPC_GENS_DIR}
            --cpp_out=${GRPC_PROTO_GRPC_GENS_DIR}
            -I ${PROTO_BASE_DIR}
            --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
            ${FIL}
    )

    message(${FIL})

    list(APPEND PROTO_SRCS "${GRPC_PROTO_GRPC_GENS_DIR}/${FIL_WE}.pb.cc")
    list(APPEND PROTO_HDRS "${GRPC_PROTO_GRPC_GENS_DIR}/${FIL_WE}.pb.h")
    list(APPEND PROTO_SRCS "${GRPC_PROTO_GRPC_GENS_DIR}/${FIL_WE}.grpc.pb.cc")
    list(APPEND PROTO_HDRS "${GRPC_PROTO_GRPC_GENS_DIR}/${FIL_WE}.grpc.pb.h")
endforeach()

# 라이브러리 추가
add_library(proto_lib
        SHARED ${PROTO_SRCS} ${PROTO_HDRS}
)

target_link_libraries(proto_lib
        grpc++
        libprotobuf
        android
        log
)

# 소스 파일 추가
add_library(native_lib SHARED
        jni_helper.cpp
        jvm/JvmManager.cpp
        CaptureThread.cpp
        grpc/GrpcServer.cpp
        grpc/VideoService.cpp
        grpc/InputService.cpp
        grpc/RestartService.cpp
        ${PROTO_SRCS}
        ${PROTO_HDRS}
)

# 타겟 링크 설정
target_link_libraries(native_lib
        PRIVATE grpc++
        PRIVATE libprotobuf
        PRIVATE proto_lib
        PRIVATE opencv_java4
        PRIVATE libuvc
        PRIVATE libjpeg
        PRIVATE libusb
        PRIVATE libUVCCamera
        PRIVATE log
        PRIVATE jnigraphics
        PRIVATE android
)

# IMPORTED 라이브러리 설정
add_library(opencv_java4 SHARED IMPORTED)
set_target_properties(opencv_java4 PROPERTIES IMPORTED_LOCATION ${pathLIBOPENCV_JAVA})

add_library(libuvc SHARED IMPORTED)
set_target_properties(libuvc PROPERTIES IMPORTED_LOCATION ${libPath}/libuvc.so)

add_library(libjpeg SHARED IMPORTED)
set_target_properties(libjpeg PROPERTIES IMPORTED_LOCATION ${libPath}/libjpeg-turbo1500.so)

add_library(libusb SHARED IMPORTED)
set_target_properties(libusb PROPERTIES IMPORTED_LOCATION ${libPath}/libusb100.so)

add_library(libUVCCamera SHARED IMPORTED)
set_target_properties(libUVCCamera PROPERTIES IMPORTED_LOCATION ${libPath}/libUVCCamera.so)
