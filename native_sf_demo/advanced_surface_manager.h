/*
 * 2025.10.30 created by jxfeng
 */
#ifndef __ADVANCED_SURFACE_MANAGER_H
#define __ADVANCED_SURFACE_MANAGER_H

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <hardware/gralloc.h>
#include <ui/GraphicBuffer.h>
#include <utils/Log.h>
#include <cutils/properties.h>
#include <signal.h>
#include <getopt.h>
#include <android/native_window.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/SurfaceControl.h>
#include <gui/Surface.h>  // 添加这个头文件
#include <thread>
#include <vector>
#include <memory>
#include "vsync_frame_controller.h"

extern void picasso_log_internal(const char *tag, const char *class_, const char *func, int level, const char *fmt, ...);

#define u8__CLASS__ ""
#define CONCAT(x) (const char*)u8##x
#define CONCAT2(x) CONCAT(x)
#define JLOGV(fmt, args...) picasso_log_internal(LOG_TAG, CONCAT2(__CLASS__), __func__, ANDROID_LOG_VERBOSE, fmt, ## args)
#define JLOGD(fmt, args...) picasso_log_internal(LOG_TAG, CONCAT2(__CLASS__), __func__, ANDROID_LOG_DEBUG, fmt, ## args)
#define JLOGI(fmt, args...) picasso_log_internal(LOG_TAG, CONCAT2(__CLASS__), __func__, ANDROID_LOG_INFO, fmt, ## args)
#define JLOGE(fmt, args...) picasso_log_internal(LOG_TAG, CONCAT2(__CLASS__), __func__, ANDROID_LOG_ERROR, fmt, ## args)

using namespace android;

/**
 * Surface配置参数
 */
struct SurfaceConfig {
    String8 name;
    int width;
    int height;
    int x;
    int y;
    int layer;
    float alpha;           // 透明度：0.0(完全透明) ~ 1.0(完全不透明)
    int type;              // Surface类型
    uint32_t color;        // 基础颜色
    bool enableBlur;       // 是否启用模糊效果
};

/**
 * AdvancedSurfaceManager - 支持类型和透明度设置的Surface管理器
 */
class AdvancedSurfaceManager : public RefBase {
public:
    AdvancedSurfaceManager();
    ~AdvancedSurfaceManager();

    // 创建具有不同类型和透明度的Surface
    status_t createAdvancedSurfaces();
    bool initialize();
    // 动态更新透明度
    void setSurfaceAlpha(int surfaceIndex, float alpha);

    // 动态更新位置
    void setSurfacePosition(int surfaceIndex, int x, int y);

    // 动态更新类型（需要重新创建Surface）
    void changeSurfaceType(int surfaceIndex, int newType);

    // 开始/停止渲染
    void startRendering();
    void stopRendering();

    void setTargetFPS(int fps);

private:
    void renderSurface(int surfaceIndex);
    void fillRGBA8Buffer(uint8_t* img, int width, int height, int stride, uint32_t color, float alpha);

    std::vector<sp<SurfaceControl>> mSurfaceControls;
    std::vector<SurfaceConfig> mSurfaceConfigs;
    std::unique_ptr<VSyncFrameController> mVSyncController;
    bool mRendering;
    // std::vector<std::thread> mRenderThreads;
    sp<SurfaceComposerClient> mClient;
    int mFrameCount;
    int64_t mStartTime;
    int mTargetFPS;
};

#endif
