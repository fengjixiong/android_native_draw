#include <android/native_activity.h>
#include <android/native_window.h>
#include "native_app_glue/android_native_app_glue.h"
#include <android/log.h>
#include <pthread.h>
#include <cstdint>
#include <unistd.h>
#include <string.h>

#define LOG_TAG "NativeWindowDemo"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct Engine {
    ANativeActivity* activity;
    ANativeWindow* window;
    pthread_mutex_t mutex;
    bool isRunning;
};

// 绘制纯色
static void drawSolidColor(Engine* engine, uint32_t color) {
	LOGI("drawSolidColor enter");
    if (!engine->window) {
		LOGE("window is null");
		return;
	}
    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(engine->window, &buffer, nullptr) != 0) {
        LOGE("Failed to lock window buffer");
        return;
    }

    uint32_t* pixels = static_cast<uint32_t*>(buffer.bits);
    int32_t stride = buffer.stride;
    for (int y = 0; y < buffer.height; ++y) {
        for (int x = 0; x < buffer.width; ++x) {
            pixels[y * stride + x] = color;
        }
    }

    ANativeWindow_unlockAndPost(engine->window);
	LOGI("drawSolidColor exit");
}

// 窗口创建/尺寸变化处理
static void handleWindowResize(Engine* engine) {
    pthread_mutex_lock(&engine->mutex);
    if (engine->window) {
        int32_t width = ANativeWindow_getWidth(engine->window);
        int32_t height = ANativeWindow_getHeight(engine->window);
        int result = ANativeWindow_setBuffersGeometry(
            engine->window, width, height, AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM
        );
        if (result == 0) {
            LOGI("Window resized to %dx%d", width, height);
            drawSolidColor(engine, 0xFF0000FF); // 蓝色
        } else {
            LOGE("Failed to set buffer geometry");
        }
    }
    pthread_mutex_unlock(&engine->mutex);
}

// 统一事件处理回调（关键修改）
static void onAppCmd(struct android_app* app, int32_t cmd) {
	LOGI("onAppCmd cmd=0x%X", cmd);
    Engine* engine = static_cast<Engine*>(app->userData);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            // 窗口初始化
            engine->window = app->window;
            handleWindowResize(engine);
            break;
        case APP_CMD_TERM_WINDOW:
            // 窗口销毁
            pthread_mutex_lock(&engine->mutex);
            if (engine->window) {
                ANativeWindow_release(engine->window);
                engine->window = nullptr;
            }
            pthread_mutex_unlock(&engine->mutex);
            break;
        case APP_CMD_PAUSE:
            // 暂停
            LOGI("Activity paused");
            break;
        case APP_CMD_RESUME:
            // 恢复
            LOGI("Activity resumed");
            break;
        case APP_CMD_DESTROY:
			LOGI("destroy");
            // 1. 释放 ANativeWindow（避免窗口资源泄漏）
            if (engine->window != nullptr) {
                ANativeWindow_release(engine->window);
                engine->window = nullptr;
            }
            // 2. 销毁互斥锁（避免线程资源泄漏）
            pthread_mutex_destroy(&engine->mutex);
            // 3. 释放 Engine 实例（关键：销毁 new 出来的对象）
            delete engine;
            app->userData = nullptr;  // 清空指针，避免野指针
            engine->isRunning = false;  // 标记退出事件循环
            __android_log_print(ANDROID_LOG_INFO, "NativeWindowDemo", "Engine destroyed");
            break;
        case APP_CMD_CONFIG_CHANGED:
            // 配置变化（如旋转）
            LOGI("Configuration changed");
            handleWindowResize(engine); // 重新适配窗口尺寸
            break;
        default:
            break;
    }
}

// 入口函数
extern "C" void android_main(struct android_app* app) {
	LOGI("android_main enter");
    // 1. 从 ANativeActivity 中获取之前初始化的 Engine 实例
    //Engine* engine = static_cast<Engine*>(app->activity->instance);
	Engine* engine = new Engine();
	// 2. 初始化 Engine 成员
    engine->activity = app->activity;
    engine->window = nullptr;
    engine->isRunning = true;
    pthread_mutex_init(&engine->mutex, nullptr);  // 初始化互斥锁

    if (engine == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "NativeWindowDemo", "Engine is null!");
        return;
    }

    // 2. 将 Engine 绑定到 android_app 的 userData（供 onAppCmd 使用）
    app->userData = engine;
    // 3. 绑定事件回调（确保 onAppCmd 能被触发）
    app->onAppCmd = onAppCmd;

    // 4. 事件循环（保持原逻辑，检测 isRunning 退出）
    while (engine->isRunning) {
        int events;
        android_poll_source* source;
        // 处理待触发事件
        while ((ALooper_pollOnce(engine->window != nullptr ? 0 : -1, nullptr, &events, (void**)&source)) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }
            // 系统请求销毁时，退出循环
            if (app->destroyRequested != 0) {
                engine->isRunning = false;
                break;
            }
        }

        // 绘制逻辑（保持原逻辑，加锁访问窗口）
        pthread_mutex_lock(&engine->mutex);
        if (engine->window != nullptr) {
            drawSolidColor(engine, 0xFF0000FF);  // 绘制蓝色
        }
        pthread_mutex_unlock(&engine->mutex);

        usleep(1000000);  // 控制帧率（约 60 FPS）
    }
}
