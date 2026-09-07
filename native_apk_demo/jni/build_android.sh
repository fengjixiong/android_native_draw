rm -r obj

export GIT_COMMIT_ID=`git rev-parse --short HEAD`
export BUILD_DATE=`date +'%Y-%m-%d.%H:%M:%S'`

# NDK_PATH=/opt/android-ndk-r13
ndk-build -B $@ NDK_PROJECT_PATH=./ \
    NDK_APPLICATION_MK=Application.mk \
    APP_BUILD_SCRIPT=Android.mk \

cp -r libs/arm64-v8a ../build/apk/lib