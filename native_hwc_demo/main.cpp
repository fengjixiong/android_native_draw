#include "PureColorDisplay.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

void displayColors(PureColorDisplay& display) {
    while (true) {
        // 显示红色
        std::cout << "Displaying RED..." << std::endl;
        if (display.displaySolidColor(255.0f, 0.0f, 0.0f, 255.0f)) {
            std::this_thread::sleep_for(2s);
        }

        // 显示绿色
        std::cout << "Displaying GREEN..." << std::endl;
        if (display.displaySolidColor(0.0f, 255.0f, 0.0f, 255.0f)) {
            std::this_thread::sleep_for(2s);
        }

        // 显示蓝色
        std::cout << "Displaying BLUE..." << std::endl;
        if (display.displaySolidColor(0.0f, 0.0f, 255.0f, 255.0f)) {
            std::this_thread::sleep_for(2s);
        }

        // 显示白色
        std::cout << "Displaying WHITE..." << std::endl;
        if (display.displaySolidColor(255.0f, 255.0f, 255.0f, 255.0f)) {
            std::this_thread::sleep_for(2s);
        }

        // 显示黑色
        std::cout << "Displaying BLACK..." << std::endl;
        if (display.displaySolidColor(0.0f, 0.0f, 0.0f, 255.0f)) {
            std::this_thread::sleep_for(2s);
        }
    }
}

int main() {
    std::cout << "Starting Pure Color Display Demo...v1.5" << std::endl;

    // 启动 binder 线程池
    ABinderProcess_startThreadPool();

    PureColorDisplay display;

    if (!display.initialize()) {
        std::cerr << "Failed to initialize display" << std::endl;
        return -1;
    }

    std::cout << "Display initialized successfully" << std::endl;
    std::cout << "Displaying colors for 10 seconds..." << std::endl;

    // 显示各种颜色
    displayColors(display);

    std::cout << "Demo completed" << std::endl;
    return 0;
}