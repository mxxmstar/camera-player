@echo off
setlocal enabledelayedexpansion

REM Clean SAFE_RM from PATH
set "NEWPATH="
for %%p in ("%PATH:;=";"%") do (
    echo %%p | findstr /i "SAFE_RM" >nul
    if errorlevel 1 (
        if "!NEWPATH!"=="" (
            set "NEWPATH=%%~p"
        ) else (
            set "NEWPATH=!NEWPATH!;%%~p"
        )
    )
)
set "PATH=!NEWPATH!"

REM Remove SAFE_RM env vars
for /f "delims==" %%a in ('set SAFE_RM 2^>nul') do set "%%a="

cd /d e:\project\camera-player

REM Clean and rebuild
if exist build rmdir /s /q build

echo === Configuring ===
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S . -B build -DCMAKE_BUILD_TYPE=Debug
if %ERRORLEVEL% neq 0 (
    echo CONFIGURE FAILED
    exit /b 1
)

echo === Building ===
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build --config Debug
if %ERRORLEVEL% neq 0 (
    echo BUILD FAILED
    exit /b 1
)

echo === SUCCESS ===