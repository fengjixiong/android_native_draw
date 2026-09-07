/*
 * 2025.10.30 created by jxfeng
 * 2025.10.31 add multi-layer
 */

#include "advanced_surface_manager.h"
#define __CLASS__ "(main)"

// 全局变量
bool mQuit = false;
sp<AdvancedSurfaceManager> gSurfaceManager;

void sighandler(int num) {
    if (num == SIGINT) {
        JLOGI("SIGINT received, stopping...");
        mQuit = true;
        if (gSurfaceManager != nullptr) {
            gSurfaceManager->stopRendering();
        }
    }
}

int main(int argc, char** argv) {
    JLOGD("enter");
    signal(SIGINT, sighandler);

    // 初始化Binder
    sp<ProcessState> proc(ProcessState::self());
    proc->startThreadPool();

    JLOGI("Starting Surface Type and Alpha Demo");

    // 创建Surface管理器
    gSurfaceManager = new AdvancedSurfaceManager();

    // 创建Surface
    bool result = gSurfaceManager->initialize();
    if (result != true) {
        JLOGE("Failed to create surfaces");
        return EXIT_FAILURE;
    }

    // 启动渲染
    gSurfaceManager->startRendering();

    JLOGI("Demo started successfully!");
    JLOGI("Created 3 surfaces with different transparency levels:");
    JLOGI("  - Surface 0: 70%% opaque (Red with breathing effect)");
    JLOGI("  - Surface 1: 30%% opaque (Green)");
    JLOGI("  - Surface 2: 100%% opaque (Blue)");
    JLOGI("Press Ctrl+C to exit");

    // 主循环：演示动态透明度变化
    int animationFrame = 0;
    while (!mQuit) {
        usleep(100000); // 1s

        // 动态改变第一个Surface的透明度
        if (gSurfaceManager != nullptr) {
            float alpha = 0.5f + 0.5f * sin(animationFrame * 0.1f);
            gSurfaceManager->setSurfaceAlpha(0, alpha);

            // 动态移动第二个Surface
            int x = 200 + 80 * sin(animationFrame * 0.05f);
            int y = 400 + 200 * cos(animationFrame * 0.05f);
            gSurfaceManager->setSurfacePosition(1, x, y);
        }

        animationFrame++;
    }

    // 停止渲染
    gSurfaceManager->stopRendering();

    JLOGI("Surface Type and Alpha Demo exited");
    return EXIT_SUCCESS;
}
