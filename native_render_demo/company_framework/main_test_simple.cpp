#include <stdio.h>
#include <unistd.h>
#include <android/log.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "company_framework_helper.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "CompanyFramework", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "CompanyFramework", __VA_ARGS__)

using namespace company;

int BufferFillWithColor(CompanyFrameworkHelper* framework,
    buffer_handle buffer, int color_argb) {

    LOGD("%s enter", __func__);

    int height = framework->BufferGetHeight(buffer);
    int stride = framework->BufferGetStride(buffer);
    int width  = framework->BufferGetWidth(buffer);
    uint32_t* pixels = (uint32_t*)framework->BufferLock(buffer);
    if (pixels == NULL) {
        LOGD("%s error: buffer lock fail", __func__);
        return -1;
    }
    for (int y=0; y<height; y++) {
        for (int x=0; x<width; x++) {
            pixels[y * stride + x] = color_argb;
        }
    }
    framework->BufferUnlock(buffer);
    return 0;
}

int dump_buffer(CompanyFrameworkHelper* framework, buffer_handle buffer, int platform, const char* dump_file) {
    LOGD("%s enter", __func__);
    int32_t fd = framework->BufferGetFd(buffer, platform);
    int32_t size = (int32_t)framework->BufferGetTotalSize(buffer);

    if (fd <0 || size <=0) {
        LOGE("%s error fd and size: fd=%d, size=%d", __func__, fd, size);
        return -1;
    }

    LOGD("dump to %s", dump_file);
    void* addr = ::mmap(nullptr, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        LOGE("%s mmap failed", __func__);
        return -1;
    }
    FILE* fp = fopen(dump_file, "wb");
    if (fp == NULL) {
        LOGE("%s cannot open file: %s", __func__, dump_file);
        return -1;
    }
    fwrite(addr, size, sizeof(char), fp);
    fclose(fp);
    ::munmap(addr, size);

    return 0;
}

int Test_CPU_Buffer(CompanyFrameworkHelper* framework, int platform, int width, int height) {
    LOGD("%s enter", __func__);

    // 创建 Surface
    int surface_flags = 0;
    surface_handle surface1 = framework->SurfaceCreate(
        "CPUSurface1", width, height, COMPANY_HAL_PIXEL_FORMAT_RGBA_8888, surface_flags, 0, 0, 100001
    );
    LOGD("CPUSurface1: %d", surface1);
    surface_handle surface2 = framework->SurfaceCreate(
        "CPUSurface2", width, height, COMPANY_HAL_PIXEL_FORMAT_RGBA_8888, surface_flags, 0, 300, 100002,
        "0,1,-1,0" // 顺时针90
    );
    LOGD("CPUSurface2: %d", surface2);

    // 创建 Buffer
    buffer_handle buffer1 = framework->BufferCreate(
        "CPUBuffer1", width, height, COMPANY_HAL_PIXEL_FORMAT_RGBA_8888,
        COMPANY_GRALLOC_USAGE_HW_RENDER | COMPANY_GRALLOC_USAGE_SW_WRITE_OFTEN | COMPANY_GRALLOC_USAGE_SW_READ_OFTEN
    );
    LOGD("CPUBuffer1: %d", buffer1);
    // 创建 Buffer
    buffer_handle buffer2 = framework->BufferCreate(
        "CPUBuffer2", width, height, COMPANY_HAL_PIXEL_FORMAT_RGBA_8888,
        COMPANY_GRALLOC_USAGE_HW_RENDER | COMPANY_GRALLOC_USAGE_SW_WRITE_OFTEN | COMPANY_GRALLOC_USAGE_SW_READ_OFTEN
    );
    LOGD("CPUBuffer2: %d", buffer2);

    // 填充
    BufferFillWithColor(framework, buffer1, 0xFF0000FF);
    BufferFillWithColor(framework, buffer2, 0xFF00FF00);

    // 显示
    framework->SurfaceSetBuffer(surface1, buffer1);
    framework->SurfaceSetBuffer(surface2, buffer2);
    LOGD("1 显示 2 秒...");
    sleep(2);

    // 填充
    BufferFillWithColor(framework, buffer1, 0xFF00FF00);
    BufferFillWithColor(framework, buffer2, 0xFF0000FF);
    LOGD("2 显示 2 秒...");
    sleep(2);

    dump_buffer(framework, buffer1, platform, "dump_cpu_buffer_1.raw");

    framework->SurfaceSetBuffer(surface1, buffer1);
    framework->SurfaceSetBuffer(surface2, buffer2);
    LOGD("3 显示 2 秒...");
    sleep(2);

    // 释放
    framework->SurfaceDelete(surface1);
    framework->SurfaceDelete(surface2);
    framework->BufferDelete(buffer1);
    framework->BufferDelete(buffer2);
    sleep(2);
    LOGD("%s exit", __func__);

    return 0;
}

int Test_GPU_Buffer(CompanyFrameworkHelper* framework, int platform, int width, int height) {
    LOGD("%s enter", __func__);

    // 创建 Surface
    int surface_flags = 0;
    surface_handle surface = framework->SurfaceCreate(
        "GPUSerface", width, height, COMPANY_HAL_PIXEL_FORMAT_RGBA_8888, surface_flags, 0, 600, 100001
    );
    LOGD("GPUSerface: %d", surface);

    // 生成 原始数据
    int img_size = width * height;
    uint32_t *pixels = (uint32_t*)malloc(img_size * sizeof(uint32_t));
    if (pixels != nullptr) {
        for (int i = 0; i < img_size; i++)
            pixels[i] = 0xFFFF0000;
    } else {
        LOGE("%s malloc fail!", __func__);
        framework->SurfaceDelete(surface);
        return -1;
    }

    // 创建 Buffer
    buffer_handle buffer = framework->BufferCreate(
        "GPUBuffer", width, height, COMPANY_HAL_PIXEL_FORMAT_RGBA_8888,
        0,
        pixels, 1
    );
    LOGD("GPUBuffer: %d", buffer);

    // dump
    dump_buffer(framework, buffer, platform, "dump_gpu_buffer_1.raw");

    // 显示
    framework->SurfaceSetBuffer(surface, buffer);
    LOGD("3 显示 5 秒...");
    sleep(5);

    // 释放
    framework->SurfaceDelete(surface);
    framework->BufferDelete(buffer);
    if (pixels != nullptr) {
        free(pixels);
    }
    sleep(2);
    LOGD("%s exit", __func__);

    return 0;
}

int main(int argc, const char**argv) {
    LOGD("=== NDK 测试程序启动 ===");
    int platform = 0;
    int width = 400;
    int height = 200;

    if (argc > 1)
        platform = atoi(argv[1]);
    if (argc > 2)
        width = atoi(argv[2]);
    if (argc > 3)
        height = atoi(argv[3]);

    CompanyFrameworkHelper* framework = CompanyFrameworkHelper::GetInstance();

    Test_CPU_Buffer(framework, platform, width, height);
    Test_GPU_Buffer(framework, platform, width, height);

    LOGD("=== 测试结束 ===");
    return 0;
}