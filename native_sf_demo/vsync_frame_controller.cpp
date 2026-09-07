#include "vsync_frame_controller.h"

#define __CLASS__ "VSyncFrameControl"

VSyncFrameController::VSyncFrameController(FrameRate fps)
    : mFrameRate(fps),
      mFrameIntervalNs(1000000000LL / static_cast<int64_t>(fps)),
      mRunning(false),
      mLastFrameTime(0) {
}

VSyncFrameController::~VSyncFrameController() {
    stop();
}

bool VSyncFrameController::initialize() {
    mReceiver = std::make_unique<DisplayEventReceiver>();
    status_t status = mReceiver->initCheck();
    if (status != NO_ERROR) {
        ALOGE("Failed to initialize DisplayEventReceiver: %d", status);
        return false;
    }

    // 请求 VSYNC 事件
    mReceiver->setVsyncRate(1);
    ALOGI("VSyncFrameController initialized for %d FPS", mFrameRate);
    return true;
}

void VSyncFrameController::start() {
    if (mRunning) return;

    mRunning = true;
    mVSyncThread = std::thread(&VSyncFrameController::vsyncThreadLoop, this);
    ALOGI("VSyncFrameController started");
}

void VSyncFrameController::stop() {
    mRunning = false;
    if (mVSyncThread.joinable()) {
        mVSyncThread.join();
    }
    ALOGI("VSyncFrameController stopped");
}

void VSyncFrameController::setFrameRate(FrameRate fps) {
    mFrameRate = fps;
    mFrameIntervalNs = 1000000000LL / static_cast<int64_t>(fps);
    ALOGI("Frame rate changed to %d FPS", fps);
}

void VSyncFrameController::vsyncThreadLoop() {
    ALOGI("VSync thread started");

    // 请求第一个 VSYNC
    scheduleNextVSync();

    while (mRunning) {
        DisplayEventReceiver::Event events[16];
        ssize_t n;

        // 等待 VSYNC 事件
        n = mReceiver->getEvents(events, 16);
        if (n < 0) {
            ALOGE("Error reading events: %zd", n);
            break;
        }

        for (int i = 0; i < n; i++) {
            if (events[i].header.type == DisplayEventReceiver::DISPLAY_EVENT_VSYNC) {
                handleVSync(0);
            }
        }
    }

    ALOGI("VSync thread exited");
}

void VSyncFrameController::handleVSync(int64_t timestamp) {
    int64_t currentTime = systemTime(SYSTEM_TIME_MONOTONIC);

    // 计算是否应该渲染这一帧
    if (mLastFrameTime == 0 || (currentTime - mLastFrameTime) >= mFrameIntervalNs) {
        mLastFrameTime = currentTime;

        // 调用渲染回调
        if (mRenderCallback) {
            mRenderCallback(currentTime);
        }
    }

    // 请求下一个 VSYNC
    scheduleNextVSync();
}

void VSyncFrameController::scheduleNextVSync() {
    if (mReceiver) {
        mReceiver->requestNextVsync();
    }
}

// // 简化的渲染器类
// class SimpleVSyncRenderer : public RefBase {
// public:
//     SimpleVSyncRenderer();
//     virtual ~SimpleVSyncRenderer();

//     bool initialize();
//     void startRendering();
//     void stopRendering();
//     void setTargetFPS(int fps);

// private:
//     void createSurface();
//     void renderFrame(int64_t frameTime);

//     sp<SurfaceComposerClient> mClient;
//     sp<SurfaceControl> mSurfaceControl;
//     std::unique_ptr<VSyncFrameController> mVSyncController;
//     std::atomic<bool> mRendering;
//     int mFrameCount;
//     int64_t mStartTime;
//     int mTargetFPS;
// };

// SimpleVSyncRenderer::SimpleVSyncRenderer()
//     : mRendering(false),
//       mFrameCount(0),
//       mStartTime(0),
//       mTargetFPS(60) {
// }

// SimpleVSyncRenderer::~SimpleVSyncRenderer() {
//     stopRendering();
// }

// bool SimpleVSyncRenderer::initialize() {
//     mClient = new SurfaceComposerClient();
//     if (mClient->initCheck() != NO_ERROR) {
//         ALOGE("Failed to initialize SurfaceComposerClient");
//         return false;
//     }

//     createSurface();

//     // 初始化 VSync 控制器
//     mVSyncController = std::make_unique<VSyncFrameController>(
//         static_cast<VSyncFrameController::FrameRate>(mTargetFPS));

//     if (!mVSyncController->initialize()) {
//         ALOGE("Failed to initialize VSync controller");
//         return false;
//     }

//     // 设置渲染回调
//     mVSyncController->setRenderCallback([this](int64_t frameTime) {
//         this->renderFrame(frameTime);
//     });

//     return true;
// }

// void SimpleVSyncRenderer::createSurface() {
//     int width = 800;
//     int height = 600;

//     mSurfaceControl = mClient->createSurface(
//         String8("VSyncControlledSurface"),
//         width,
//         height,
//         PIXEL_FORMAT_RGBA_8888,
//         ISurfaceComposerClient::eFXSurfaceBufferState
//     );

//     if (mSurfaceControl != nullptr) {
//         SurfaceComposerClient::Transaction t;
//         t.setLayer(mSurfaceControl, INT32_MAX - 1000)
//          .setPosition(mSurfaceControl, 100, 100)
//          .show(mSurfaceControl)
//          .apply();

//         ALOGI("Surface created: %dx%d", width, height);
//     }
// }

// void SimpleVSyncRenderer::startRendering() {
//     if (mRendering) return;

//     mRendering = true;
//     mFrameCount = 0;
//     mStartTime = systemTime(SYSTEM_TIME_MONOTONIC);

//     mVSyncController->start();
//     ALOGI("Rendering started at %d FPS", mTargetFPS);
// }

// void SimpleVSyncRenderer::stopRendering() {
//     mRendering = false;
//     mVSyncController->stop();

//     // 计算实际帧率
//     int64_t endTime = systemTime(SYSTEM_TIME_MONOTONIC);
//     int64_t duration = endTime - mStartTime;
//     if (duration > 0) {
//         float actualFPS = (mFrameCount * 1000000000.0f) / duration;
//         ALOGI("Rendering stopped. Frames: %d, Duration: %.2fs, Actual FPS: %.2f",
//               mFrameCount, duration / 1000000000.0f, actualFPS);
//     }
// }

// void SimpleVSyncRenderer::setTargetFPS(int fps) {
//     mTargetFPS = fps;
//     mVSyncController->setFrameRate(static_cast<VSyncFrameController::FrameRate>(fps));
//     ALOGI("Target FPS changed to: %d", fps);
// }

// void SimpleVSyncRenderer::renderFrame(int64_t frameTime) {
//     if (!mRendering || mSurfaceControl == nullptr) return;

//     sp<ANativeWindow> window = mSurfaceControl->getSurface();
//     if (window == nullptr) return;

//     ANativeWindow* nativeWindow = window.get();

//     // 配置窗口（只需要一次）
//     static bool configured = false;
//     if (!configured) {
//         native_window_api_connect(nativeWindow, NATIVE_WINDOW_API_CPU);
//         native_window_set_buffers_format(nativeWindow, PIXEL_FORMAT_RGBA_8888);
//         native_window_set_usage(nativeWindow, GRALLOC_USAGE_SW_WRITE_OFTEN);
//         native_window_set_buffer_count(nativeWindow, 3);
//         configured = true;
//     }

//     // 获取缓冲区
//     ANativeWindowBuffer* buffer = nullptr;
//     int fenceFd = -1;
//     status_t err = nativeWindow->dequeueBuffer(nativeWindow, &buffer, &fenceFd);
//     if (err != NO_ERROR) {
//         ALOGE("Failed to dequeue buffer: %d", err);
//         return;
//     }

//     if (fenceFd >= 0) {
//         sp<Fence> fence(new Fence(fenceFd));
//         fence->wait(5000);
//     }

//     sp<GraphicBuffer> graphicBuffer = GraphicBuffer::from(buffer);

//     // 锁定缓冲区
//     uint8_t* imageData = nullptr;
//     err = graphicBuffer->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, reinterpret_cast<void**>(&imageData));
//     if (err != NO_ERROR) {
//         ALOGE("Failed to lock buffer: %d", err);
//         nativeWindow->cancelBuffer(nativeWindow, buffer, -1);
//         return;
//     }

//     // 渲染内容
//     int width = graphicBuffer->getWidth();
//     int height = graphicBuffer->getHeight();
//     int stride = graphicBuffer->getStride();

//     // 创建动画效果
//     float time = mFrameCount * 0.05f;
//     uint8_t r = static_cast<uint8_t>((sin(time) + 1.0f) * 127.5f);
//     uint8_t g = static_cast<uint8_t>((sin(time + 2.0f) + 1.0f) * 127.5f);
//     uint8_t b = static_cast<uint8_t>((sin(time + 4.0f) + 1.0f) * 127.5f);

//     // 填充颜色
//     for (int y = 0; y < height; y++) {
//         for (int x = 0; x < width; x++) {
//             uint8_t* pixel = imageData + (4 * (y * stride + x));
//             pixel[0] = r;
//             pixel[1] = g;
//             pixel[2] = b;
//             pixel[3] = 255;
//         }
//     }

//     graphicBuffer->unlock();
//     nativeWindow->queueBuffer(nativeWindow, buffer, -1);

//     // 更新帧统计
//     mFrameCount++;
//     if (mFrameCount % 60 == 0) {
//         int64_t currentTime = systemTime(SYSTEM_TIME_MONOTONIC);
//         int64_t duration = currentTime - mStartTime;
//         if (duration > 0) {
//             float fps = (mFrameCount * 1000000000.0f) / duration;
//             ALOGI("Frame: %d, FPS: %.2f", mFrameCount, fps);
//         }
//     }
// }

// // 全局变量
// bool mQuit = false;

// void sighandler(int num) {
//     if (num == SIGINT) {
//         ALOGI("SIGINT received, stopping...");
//         mQuit = true;
//     }
// }

// int main(int argc, char** argv) {
//     signal(SIGINT, sighandler);

//     sp<ProcessState> proc(ProcessState::self());
//     proc->startThreadPool();

//     ALOGI("Starting VSync Frame Rate Control Demo");

//     // 创建渲染器
//     sp<SimpleVSyncRenderer> renderer = new SimpleVSyncRenderer();

//     if (!renderer->initialize()) {
//         ALOGE("Failed to initialize renderer");
//         return EXIT_FAILURE;
//     }

//     // 启动渲染
//     renderer->startRendering();

//     ALOGI("Renderer started. Press Ctrl+C to exit");
//     ALOGI("Commands: 1=30FPS, 2=60FPS, 3=90FPS, 4=120FPS");

//     // 简单的命令行控制
//     while (!mQuit) {
//         usleep(100000); // 100ms

//         // 这里可以添加其他控制逻辑
//         // 在实际应用中，可以通过其他方式接收用户输入
//     }

//     // 停止渲染
//     renderer->stopRendering();

//     ALOGI("VSync Frame Rate Control Demo exited");
//     return EXIT_SUCCESS;
// }