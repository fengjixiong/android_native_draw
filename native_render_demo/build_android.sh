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

cp out/libs/arm64-v8a/*    prebuild_libs/arm64-v8a/*
cp out/libs/armeabi-v7a/*  prebuild_libs/armeabi-v7a/*
cp out/libs/x86_64/*       prebuild_libs/x86_64/*
