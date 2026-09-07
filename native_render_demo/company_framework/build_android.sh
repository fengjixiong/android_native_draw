#!/bin/bash
rm -r out

# export GIT_COMMIT_ID=`git rev-parse --short HEAD`
# export BUILD_DATE=`date +'%Y-%m-%d.%H:%M:%S'`

# NDK_PATH=/opt/android-ndk-r13
ndk-build -B $@ NDK_PROJECT_PATH=./ \
    NDK_APPLICATION_MK=Application.mk.bak \
    APP_BUILD_SCRIPT=Android.mk.bak \
    NDK_OUT=./out/obj \
    NDK_LIBS_OUT=./out/libs

if [ $? != 0 ]; then
    echo "build failed"
    exit 1
fi

mkdir -p prebuild_libs/arm64-v8a
mkdir -p prebuild_libs/armeabi-v7a
mkdir -p prebuild_libs/x86_64

# cp out/libs/arm64-v8a/libcompany_framework.so    prebuild_libs/arm64-v8a/
# cp out/libs/armeabi-v7a/libcompany_framework.so  prebuild_libs/armeabi-v7a/
cp out/libs/x86_64/libcompany_framework.so       prebuild_libs/x86_64/
cp out/libs/x86_64/company_framework_test        prebuild_libs/x86_64/

cp out/libs/x86_64/libcompany_framework.so       /home/pub/fp_database/ICNF6586/Android/android14/
cp out/libs/x86_64/company_framework_test        /home/pub/fp_database/ICNF6586/Android/android14/
