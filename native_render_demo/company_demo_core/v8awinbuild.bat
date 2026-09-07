@echo off

setlocal

::set GIT_COMMIT_ID=unknown

::for /f %%i in ('git rev-parse --short HEAD') do set GIT_COMMIT_ID=%%i

::echo Git Commit ID: %GIT_COMMIT_ID%

::del /s /f /q out

call ndk-build -j32 -B NDK_PROJECT_PATH=./ ^
    NDK_APPLICATION_MK=Application.mk.bak ^
    APP_BUILD_SCRIPT=Android.mk.bak ^
    NDK_OUT=./out/obj  ^
    NDK_LIBS_OUT=./out/libs

endlocal

echo F | xcopy out\obj\local\armeabi-v7a\libcompany_demo_core.so prebuilt\armeabi-v7a\libcompany_demo_core.so /y
echo F | xcopy out\obj\local\arm64-v8a\libcompany_demo_core.so prebuilt\arm64-v8a\libcompany_demo_core.so /y
echo F | xcopy out\obj\local\x86_64\libcompany_demo_core.so prebuilt\x86_64\libcompany_demo_core.so /y

echo now: %time%
