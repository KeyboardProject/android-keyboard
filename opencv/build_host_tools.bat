@echo off
setlocal enabledelayedexpansion
REM 현재 배치 파일이 위치한 디렉토리를 기준으로 작업합니다.
set CURRENT_DIR=%~dp0
set BUILD_DIR=%CURRENT_DIR%\grpc\cmake-build-debug123

REM vswhere.exe 경로 설정
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

REM vswhere.exe 존재 확인
if not exist %VSWHERE% (
    echo vswhere.exe를 찾을 수 없습니다. Visual Studio 2017 이상이 설치되어 있는지 확인해 주세요.
    exit /b 1
)

REM 최신 버전의 Visual Studio 경로 찾기
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set VS_INSTALL_DIR=%%i
)

REM VsDevCmd.bat 경로 설정
set VS_DEV_CMD="%VS_INSTALL_DIR%\Common7\Tools\VsDevCmd.bat"

REM VsDevCmd.bat 존재 확인
if not exist %VS_DEV_CMD% (
    echo VsDevCmd.bat를 찾을 수 없습니다. Visual Studio가 올바르게 설치되어 있는지 확인해 주세요.
    exit /b 1
)

REM Visual Studio 환경 설정 스크립트 호출
call %VS_DEV_CMD%
if errorlevel 1 (
    echo Failed to set up Visual Studio environment.
    exit /b 1
)

REM cmake 경로 설정 (PATH에 없을 경우)
where cmake >nul 2>&1
if errorlevel 1 (
    echo cmake가 PATH에 없습니다. cmake의 경로를 지정합니다.
    set CMAKE_EXE="C:\Program Files\CMake\bin\cmake.exe"
) else (
    set CMAKE_EXE=cmake
)

REM ninja 경로 설정 (PATH에 없을 경우)
where ninja >nul 2>&1
if errorlevel 1 (
    echo ninja가 PATH에 없습니다. ninja의 경로를 지정합니다.
    set NINJA_EXE="C:\Program Files\Ninja\ninja.exe"
) else (
    set NINJA_EXE=ninja
)

REM git 경로 설정 (PATH에 없을 경우)
where git >nul 2>&1
if errorlevel 1 (
    echo git이 PATH에 없습니다. git의 경로를 지정합니다.
    set GIT_CMD="C:\Program Files\Git\bin\git.exe"
) else (
    set GIT_CMD=git
)

REM 빌드 디렉토리 생성
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
)

REM 아키텍처 감지 (64비트 환경이라 가정하고, 필요시 다른 아키텍처도 설정 가능)
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set CMAKE_ARCHITECTURE=x64
) else (
    set CMAKE_ARCHITECTURE=x86
)

REM grpc 서브모듈 업데이트
cd "%CURRENT_DIR%\grpc"
%GIT_CMD% submodule update --init --recursive
if errorlevel 1 (
    echo 서브모듈 업데이트에 실패했습니다.
    exit /b 1
)

REM 긴 CMake 구성 명령을 변수에 저장
set CMAKE_CONFIG_CMD=%CMAKE_EXE% -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=%NINJA_EXE% -G Ninja -S "%CURRENT_DIR%\grpc" -B "%BUILD_DIR%"

REM CMake 구성 단계 실행
call !CMAKE_CONFIG_CMD!
if errorlevel 1 (
    echo CMake 구성에 실패했습니다.
    exit /b 1
)

REM protoc 및 grpc_cpp_plugin 빌드
set CMAKE_BUILD_CMD=%CMAKE_EXE% --build "%BUILD_DIR%" --target protoc grpc_cpp_plugin -j 14
call !CMAKE_BUILD_CMD!
if errorlevel 1 (
    echo protoc 및 grpc_cpp_plugin 빌드에 실패했습니다.
    exit /b 1
)

REM 빌드된 실행 파일을 tools 디렉토리로 복사
if not exist "%CURRENT_DIR%\tools" (
    mkdir "%CURRENT_DIR%\tools"
)

copy "%BUILD_DIR%\third_party\protobuf\protoc.exe" "%CURRENT_DIR%\tools\"
if errorlevel 1 (
    echo protoc.exe 복사에 실패했습니다.
    exit /b 1
)

copy "%BUILD_DIR%\grpc_cpp_plugin.exe" "%CURRENT_DIR%\tools\"
if errorlevel 1 (
    echo grpc_cpp_plugin.exe 복사에 실패했습니다.
    exit /b 1
)

echo Host tools have been built and copied to the tools directory.
