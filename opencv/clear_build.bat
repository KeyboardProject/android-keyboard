@echo off
set CURRENT_DIR=%~dp0
set BUILD_DIR=%CURRENT_DIR%\grpc\cmake-build-debug
set TOOLS_DIR=%CURRENT_DIR%\tools

REM 빌드 디렉토리 삭제
if exist "%BUILD_DIR%" (
    echo Deleting build directory: %BUILD_DIR%
    rmdir /S /Q "%BUILD_DIR%"
) else (
    echo Build directory does not exist: %BUILD_DIR%
)

REM tools 디렉토리 삭제
if exist "%TOOLS_DIR%" (
    echo Deleting tools directory: %TOOLS_DIR%
    rmdir /S /Q "%TOOLS_DIR%"
) else (
    echo Tools directory does not exist: %TOOLS_DIR%
)

echo Clean-up complete. Build directories removed.
