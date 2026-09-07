/*
 * VSync Frame Rate Control - 完整的自定义 VSync 控制器
 */
#ifndef __VSyncFrameController_H_
#define __VSyncFrameController_H_

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
#include <gui/Surface.h>
#include <ui/LayerStack.h>
#include <gui/ISurfaceComposer.h>
#include <gui/DisplayEventReceiver.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>

using namespace android;
using namespace std::chrono;

// 自定义的 VSync 帧率控制器
class VSyncFrameController {
public:
    enum FrameRate {
        FPS_30 = 30,
        FPS_60 = 60,
        FPS_90 = 90,
        FPS_120 = 120
    };

    VSyncFrameController(FrameRate fps = FPS_60);
    virtual ~VSyncFrameController();
    
    bool initialize();
    void start();
    void stop();
    void setFrameRate(FrameRate fps);
    
    // 渲染回调函数
    void setRenderCallback(std::function<void(int64_t frameTimeNs)> callback) {
        mRenderCallback = callback;
    }

private:
    void vsyncThreadLoop();
    void handleVSync(int64_t timestamp);
    void scheduleNextVSync();
    
    FrameRate mFrameRate;
    int64_t mFrameIntervalNs;
    std::thread mVSyncThread;
    std::atomic<bool> mRunning;
    std::unique_ptr<DisplayEventReceiver> mReceiver;
    std::function<void(int64_t)> mRenderCallback;
    int64_t mLastFrameTime;
};

#endif
