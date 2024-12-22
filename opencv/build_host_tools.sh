#!/bin/bash

# 현재 스크립트가 위치한 디렉토리를 기준으로 작업합니다.
CURRENT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Submodule 업데이트
echo "Initializing and updating git submodules..."
cd "${CURRENT_DIR}"
git submodule update --init --recursive
if [ $? -ne 0 ]; then
	    echo "Failed to update git submodules."
	        exit 1
fi

# Build protoc.exe
cd "${CURRENT_DIR}/grpc/third_party/protobuf"
mkdir -p build
cd build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../install ..
if [ $? -ne 0 ]; then
	    echo "CMake configuration failed for protobuf."
	        exit 1
fi
make -j$(nproc)
make install
if [ $? -ne 0 ]; then
	    echo "Build failed for protobuf."
	        exit 1
fi

# Build grpc_cpp_plugin.exe
cd "${CURRENT_DIR}/grpc"
mkdir -p build
cd build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../install ..
if [ $? -ne 0 ]; then
	    echo "CMake configuration failed for gRPC."
	        exit 1
fi
make -j$(nproc) grpc_cpp_plugin
if [ $? -ne 0 ]; then
	    echo "Build failed for grpc_cpp_plugin."
	        exit 1
fi

# 복사
mkdir -p "${CURRENT_DIR}/tools"
cp ../third_party/protobuf/install/bin/protoc "${CURRENT_DIR}/tools/"
if [ $? -ne 0 ]; then
	    echo "Failed to copy protoc."
	        exit 1
fi
cp grpc_cpp_plugin "${CURRENT_DIR}/tools/"
if [ $? -ne 0 ]; then
	    echo "Failed to copy grpc_cpp_plugin."
	        exit 1
fi

echo "Host tools have been built and copied to the tools directory."

