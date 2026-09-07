#ifndef __MAIN_RENDERENGINE_H
#define __MAIN_RENDERENGINE_H

#include <log/log.h>
#include <utils/RefBase.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <mutex>
#include <inttypes.h>   // PRIx64
#include <android/native_window.h>
#include <getopt.h>

using namespace android;
sp<GraphicBuffer> createBufferAndClean(int platform,
                    uint32_t width, uint32_t height,
                    int32_t pixel_format = HAL_PIXEL_FORMAT_RGBA_8888,
                    uint64_t usageFlags = 0,
                    std::string name = "unknown");
int loadBuffer_mmap(sp<GraphicBuffer> buffer, int platform, const char* in_file, int allow_larger_size=0);
int loadBuffer_lock(const sp<GraphicBuffer>& buffer, uint32_t rgba,
        int platform, const char* in_file, int allow_larger_size=0);
int saveBuffer_mmap(sp<GraphicBuffer> buffer, int platform, const char* out_dir);
int saveBuffer_lock(const sp<GraphicBuffer>& buffer, const char* filename);


int copyBuffer_engine(sp<GraphicBuffer> in_buffer,
                      sp<GraphicBuffer> out_buffer,
                      int renderengine_VulKan);

int copyBuffer_gles(sp<GraphicBuffer> in_buffer,
                    sp<GraphicBuffer> out_buffer,
                    int use_egl_10bit);

#endif
