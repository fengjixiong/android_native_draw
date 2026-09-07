LOCAL_PATH := $(call my-dir)

# 首先编译 native_app_glue 静态库
#include $(CLEAR_VARS)
#LOCAL_MODULE := native_app_glue
#LOCAL_SRC_FILES := native_app_glue/android_native_app_glue.c
#include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := nativewindow_demo
LOCAL_SRC_FILES := native_activity.cpp
LOCAL_LDFLAGS += -llog -landroid -lEGL -lGLESv2  # 链接日志和Android库
LOCAL_STATIC_LIBRARIES += android_native_app_glue  # 依赖Native Activity胶水库

include $(BUILD_SHARED_LIBRARY)
$(call import-module,android/native_app_glue)