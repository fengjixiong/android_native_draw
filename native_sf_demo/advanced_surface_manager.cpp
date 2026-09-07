#include "advanced_surface_manager.h"

#define __CLASS__ "AdvancedSurfaceManager"

AdvancedSurfaceManager::AdvancedSurfaceManager()
    :   mRendering(false),

        mFrameCount(0),
        mStartTime(0),
        mTargetFPS(30) {
    JLOGD("enter");
}

AdvancedSurfaceManager::~AdvancedSurfaceManager() {
    JLOGD("enter");
    stopRendering();
}

bool AdvancedSurfaceManager::initialize() {
    JLOGD("enter");
    mClient = new SurfaceComposerClient();
    status_t err = mClient->initCheck();
    if (err != NO_ERROR) {
        JLOGE("SurfaceComposerClient init failed: %d", err);
        return err;
    }

    createAdvancedSurfaces();

    // 初始化 VSync 控制器
    mVSyncController = std::make_unique<VSyncFrameController>(
        static_cast<VSyncFrameController::FrameRate>(mTargetFPS));

    if (!mVSyncController->initialize()) {
        ALOGE("Failed to initialize VSync controller");
        return false;
    }

    // 设置渲染回调
    mVSyncController->setRenderCallback([this](int64_t frameTime) {
        //this->renderFrame(frameTime);
        for (size_t i = 0; i < mSurfaceControls.size(); i++) {
            this->renderSurface(i);
        }

        // 更新帧统计
        mFrameCount++;
        if (mFrameCount % mTargetFPS == 0) {
            int64_t currentTime = systemTime(SYSTEM_TIME_MONOTONIC);
            int64_t duration = currentTime - mStartTime;
            if (duration > 0) {
                float fps = (mFrameCount * 1000000000.0f) / duration;
                ALOGI("Frame: %d, FPS: %.2f", mFrameCount, fps);
            }
        }
    });

    return true;
}

/**
 * createAdvancedSurfaces - 创建多个具有不同类型和透明度的Surface
 */
status_t AdvancedSurfaceManager::createAdvancedSurfaces() {
    JLOGD("enter");

    mSurfaceControls.clear();
    mSurfaceConfigs.clear();

    // ===== Surface 1: 普通应用Surface (半透明) =====
    SurfaceConfig config1;
    config1.name = String8("NativeSFDemo-AppSurface-Transparent");
    config1.width = 200;
    config1.height = 200;
    config1.x = 0;
    config1.y = 0;
    config1.layer = INT32_MAX - 2;
    config1.alpha = 0.9f;      // 90% 不透明
    config1.type = ISurfaceComposerClient::eFXSurfaceBufferState;
    config1.color = 0xFFFF0000; // 红色
    config1.enableBlur = false;

    // ===== Surface 2: 全透明Surface =====
    SurfaceConfig config2;
    config2.name = String8("NativeSFDemo-TransparentSurface");
    config2.width = 200;
    config2.height = 200;
    config2.x = 200;
    config2.y = 400;
    config2.layer = INT32_MAX - 1;
    config2.alpha = 0.9f;      // 90% 不透明
    config2.type = ISurfaceComposerClient::eFXSurfaceBufferState;
    config2.color = 0xFF00FF00; // 绿色
    config2.enableBlur = false;

    // ===== Surface 3: 不透明Surface =====
    SurfaceConfig config3;
    config3.name = String8("NativeSFDemo-OpaqueSurface");
    config3.width = 300;
    config3.height = 300;
    config3.x = 200;
    config3.y = 400;
    config3.layer = INT32_MAX;
    config3.alpha = 1.0f;      // 100% 不透明
    config3.type = ISurfaceComposerClient::eFXSurfaceBufferState;
    config3.color = 0xFF0000FF; // 蓝色
    config3.enableBlur = false;

    std::vector<SurfaceConfig> configs = {config1, config2, config3};
    SurfaceComposerClient::Transaction t;

    for (auto& config : configs) {
        JLOGD("Creating surface: %s, type: %d, alpha: %.2f",
              config.name.string(), config.type, config.alpha);

        // 创建Surface
        sp<SurfaceControl> surfaceControl = mClient->createSurface(
            config.name,
            config.width,
            config.height,
            PIXEL_FORMAT_RGBA_8888,
            config.type,        // 设置Surface类型
            /*parent*/ nullptr
        );

        if (surfaceControl == nullptr) {
            JLOGE("Failed to create surface: %s", config.name.string());
            continue;
        }

        // ===== 设置Surface属性 =====

        // 1. 设置层级
        t.setLayer(surfaceControl, config.layer);

        // 2. 设置位置
        t.setPosition(surfaceControl, config.x, config.y);

        // 3. 设置透明度 - 这是关键API！
        t.setAlpha(surfaceControl, config.alpha);

        // 4. 设置其他可选属性
        t.setFlags(surfaceControl,
                   layer_state_t::eLayerOpaque,
                   layer_state_t::eLayerOpaque);  // 设置不透明标志

        // 5. 显示Surface
        t.show(surfaceControl);

        mSurfaceControls.push_back(surfaceControl);
        mSurfaceConfigs.push_back(config);

        JLOGD("Successfully created and configured surface: %s", config.name.string());
    }

    // 应用所有设置
    t.apply();

    JLOGD("Created %zu surfaces with different types and alpha values", mSurfaceControls.size());
    return mSurfaceControls.empty() ? UNKNOWN_ERROR : NO_ERROR;
}

/**
 * setSurfaceAlpha - 动态设置Surface透明度
 * @param surfaceIndex: Surface索引
 * @param alpha: 透明度值 (0.0 - 1.0)
 */
void AdvancedSurfaceManager::setSurfaceAlpha(int surfaceIndex, float alpha) {
    if (surfaceIndex < 0 || surfaceIndex >= mSurfaceControls.size()) {
        JLOGE("Invalid surface index: %d", surfaceIndex);
        return;
    }

    if (alpha < 0.0f || alpha > 1.0f) {
        JLOGE("Invalid alpha value: %.2f", alpha);
        return;
    }

    // 更新配置
    mSurfaceConfigs[surfaceIndex].alpha = alpha;

    // 应用新的透明度设置
    SurfaceComposerClient::Transaction t;
    t.setAlpha(mSurfaceControls[surfaceIndex], alpha);
    t.apply();

    //JLOGD("Surface %d alpha set to: %.2f", surfaceIndex, alpha);
}

/**
 * setSurfacePosition - 动态设置Surface位置
 */
void AdvancedSurfaceManager::setSurfacePosition(int surfaceIndex, int x, int y) {
    if (surfaceIndex < 0 || surfaceIndex >= mSurfaceControls.size()) {
        JLOGE("Invalid surface index: %d", surfaceIndex);
        return;
    }

    SurfaceComposerClient::Transaction t;
    t.setPosition(mSurfaceControls[surfaceIndex], x, y);
    t.apply();

    //JLOGD("Surface %d position set to: (%d, %d)", surfaceIndex, x, y);
}

/**
 * fillRGBA8Buffer - 填充缓冲区（支持透明度）
 */
void AdvancedSurfaceManager::fillRGBA8Buffer(uint8_t* img, int width, int height,
                                           int stride, uint32_t color, float alpha) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    uint8_t a = static_cast<uint8_t>(alpha * 255); // 应用透明度

    // 创建半透明背景图案以便观察透明度效果
    bool drawPattern = (alpha < 1.0f);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t* pixel = img + (4 * (y * stride + x));

            if (drawPattern && (x / 20 + y / 20) % 2 == 0) {
                // 绘制棋盘格背景图案（用于观察透明度）
                pixel[0] = 0x80; // 灰色
                pixel[1] = 0x80;
                pixel[2] = 0x80;
                pixel[3] = 0xFF;
            } else {
                // 绘制主要颜色（应用透明度）
                pixel[0] = r;
                pixel[1] = g;
                pixel[2] = b;
                pixel[3] = a;
            }
        }
    }
}

/**
 * renderSurface - 渲染单个Surface
 */
void AdvancedSurfaceManager::renderSurface(int surfaceIndex) {
    SurfaceConfig config = mSurfaceConfigs[surfaceIndex];
    sp<SurfaceControl> surfaceControl = mSurfaceControls[surfaceIndex];

    sp<ANativeWindow> nativeWindow = surfaceControl->getSurface();
    if (nativeWindow == nullptr) {
        JLOGE("Failed to get native window for %s", config.name.string());
        return;
    }

    ANativeWindow* window = nativeWindow.get();
    status_t err = NO_ERROR;

    // 配置窗口（只需要一次）
    static bool configured_all[3] = {false, false, false};
    if (!configured_all[surfaceIndex]) {

        // 连接窗口
        err = native_window_api_connect(window, NATIVE_WINDOW_API_CPU);
        if (err != NO_ERROR) {
            JLOGE("Failed to connect %s: %d", config.name.string(), err);
            return;
        }

        // 配置窗口参数
        native_window_set_buffers_dimensions(window, config.width, config.height);
        native_window_set_buffers_format(window, PIXEL_FORMAT_RGBA_8888);
        native_window_set_usage(window, GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_HW_RENDER);

        // 设置缓冲区数量
        int minUndequeuedBuffers = 0;
        window->query(window, NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &minUndequeuedBuffers);
        native_window_set_buffer_count(window, minUndequeuedBuffers + 2);

        configured_all[surfaceIndex] = true;
    }

    //JLOGD("Starting render for %s (alpha: %.2f)", config.name.string(), config.alpha);

    // 获取缓冲区
    ANativeWindowBuffer* buffer = nullptr;
    int fenceFd = -1;

    err = window->dequeueBuffer(window, &buffer, &fenceFd);
    if (err != NO_ERROR) {
        JLOGE("%s: dequeueBuffer failed: %d", config.name.string(), err);
        return;
    }

    if (fenceFd >= 0) {
        sp<Fence> fence(new Fence(fenceFd));
        fence->wait(5000);
    }

    sp<GraphicBuffer> graphicBuffer = GraphicBuffer::from(buffer);

    // 锁定缓冲区
    uint8_t* imageData = nullptr;
    err = graphicBuffer->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, reinterpret_cast<void**>(&imageData));
    if (err != NO_ERROR) {
        JLOGE("%s: lock failed: %d", config.name.string(), err);
        window->cancelBuffer(window, buffer, -1);
        return;
    }

    // 动态效果：颜色渐变
    uint32_t currentColor = config.color;

    // 根据Surface类型应用不同的动画效果
    switch (mSurfaceConfigs[0].type) {
        case ISurfaceComposerClient::eFXSurfaceBufferState:
            // 普通Surface：颜色呼吸效果
            {
                float phase = (mFrameCount % 180) / 180.0f * 2 * M_PI;
                float brightness = 0.5f + 0.5f * sin(phase);

                uint8_t r = (currentColor >> 16) & 0xFF;
                uint8_t g = (currentColor >> 8) & 0xFF;
                uint8_t b = currentColor & 0xFF;

                r = static_cast<uint8_t>(r * brightness);
                g = static_cast<uint8_t>(g * brightness);
                b = static_cast<uint8_t>(b * brightness);

                currentColor = (r << 16) | (g << 8) | b;
            }
            break;

        default:
            // 默认效果
            break;
    }

    // 填充缓冲区（传入当前透明度）
    fillRGBA8Buffer(imageData, config.width, config.height,
                   graphicBuffer->getStride(), currentColor, config.alpha);

    graphicBuffer->unlock();

    err = window->queueBuffer(window, buffer, -1);
    if (err != NO_ERROR) {
        JLOGE("%s: queueBuffer failed: %d", config.name.string(), err);
        return;
    }
}

void AdvancedSurfaceManager::startRendering() {
    JLOGD("enter");
    if (mRendering)
        return;

    mRendering = true;
    // mRenderThreads.clear();

    mFrameCount = 0;
    mStartTime = systemTime(SYSTEM_TIME_MONOTONIC);

    mVSyncController->start();
    // // 为每个Surface创建渲染线程
    // for (size_t i = 0; i < mSurfaceControls.size(); i++) {
    //     mRenderThreads.emplace_back([this, i]() {
    //         this->renderSurface(mSurfaceConfigs[i], mSurfaceControls[i]);
    //     });
    // }
    // JLOGD("Started %zu render threads", mRenderThreads.size());
}

void AdvancedSurfaceManager::stopRendering() {
    mRendering = false;
    mVSyncController->stop();
    // for (auto& thread : mRenderThreads) {
    //     if (thread.joinable()) {
    //         thread.join();
    //     }
    // }
    // mRenderThreads.clear();
    // JLOGD("All render threads stopped");

    // 计算实际帧率
    int64_t endTime = systemTime(SYSTEM_TIME_MONOTONIC);
    int64_t duration = endTime - mStartTime;
    if (duration > 0) {
        float actualFPS = (mFrameCount * 1000000000.0f) / duration;
        ALOGI("Rendering stopped. Frames: %d, Duration: %.2fs, Actual FPS: %.2f",
              mFrameCount, duration / 1000000000.0f, actualFPS);
    }

    JLOGD("disconnect windows");
    for (size_t i = 0; i < mSurfaceControls.size(); i++) {
        sp<ANativeWindow> nativeWindow = mSurfaceControls[i]->getSurface();
        if (nativeWindow == nullptr) {
            JLOGE("Failed to get native window %d", i);
            return;
        }
        ANativeWindow* window = nativeWindow.get();
        native_window_api_disconnect(window, NATIVE_WINDOW_API_CPU);
        JLOGD("Render window %d disconnect", i);
    }

}

void AdvancedSurfaceManager::setTargetFPS(int fps) {
    mTargetFPS = fps;
    mVSyncController->setFrameRate(static_cast<VSyncFrameController::FrameRate>(fps));
    ALOGI("Target FPS changed to: %d", fps);
}


void picasso_log_internal(const char *tag, const char *class_, const char *func, int level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char g_log_buf[2048];
    vsnprintf(g_log_buf, sizeof(g_log_buf), fmt, args);

    // 通过 android log 打印的信息自带时间戳、线程号、级别
    __android_log_print(level, tag, "%s::%s %s", class_, func, g_log_buf);

    va_end(args);
}
