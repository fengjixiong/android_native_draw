#include <log/log.h>
#include <utils/RefBase.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <mutex>
#include <inttypes.h>   // PRIx64
#include <android/native_window.h>
#include <getopt.h>

using namespace android;
static int bufferGetFd(sp<GraphicBuffer> buffer, int platform) {
    struct ANativeWindowBuffer* native_buffer = buffer.get();
    //转成Android标准的handle
    const native_handle_t *buffer_handle = native_buffer->handle;
    int version = buffer_handle->version;
    int numFds  = buffer_handle->numFds;
    int numInts = buffer_handle->numInts;
    if (platform >= numFds + numInts) {
        ALOGE("%s platform(%d) exceeds handle->data[%d+%d]!", __func__, platform, numFds, numInts);
        return -1;
    } else {
        ALOGD("%s platform (%d), version (%d), numFds (%d), numInts (%d)",
            __func__, platform, version, numFds, numInts);
    }
    int fd = buffer_handle->data[platform];
    return fd;
}

static int bufferGetSize(sp<GraphicBuffer> buffer) {

    struct ANativeWindowBuffer* native_buffer = buffer.get();
    //转成Android标准的handle
    const native_handle_t *buffer_handle = native_buffer->handle;

    uint64_t totalSize = 0;
    GraphicBufferMapper::get().getAllocationSize(buffer_handle, &totalSize);
    return totalSize;
}

static uint32_t fileGetSize(const char* filepath) {
    FILE* file = fopen(filepath, "rb");
    if (file == nullptr) return -1;

    fseek(file, 0, SEEK_END);
    uint32_t size = (uint32_t)ftell(file);
    fseek(file, 0, SEEK_SET);
    fclose(file);

    return size;
}

sp<GraphicBuffer> createBufferAndClean(int platform,
                    uint32_t width, uint32_t height,
                    int32_t pixel_format, uint64_t usageFlags,
                    std::string name = "unknown")
{
    ALOGD("%s enter, w=%d, h=%d, px=%d, name=%s, usage=0x%" PRIx64,
         __func__, width, height, (int)pixel_format, name.c_str(), usageFlags);

    sp<GraphicBuffer> buffer = sp<GraphicBuffer>::make(width, height, pixel_format, 1u, usageFlags, std::move(name));
    if (buffer->initCheck() != NO_ERROR) {
        ALOGE("%s create buffer failed", __func__);
        return nullptr;
    }

    int32_t fd     = bufferGetFd(buffer, platform);
    int32_t size   = bufferGetSize(buffer);

    if (fd <0 || size <=0) {
        ALOGE("%s error condition: fd=%d, size=%d", __func__, fd, size);
        return nullptr;
    }

    void* addr = ::mmap(nullptr, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        ALOGE("mmap failed");
        return nullptr;
    }
    // 清空buffer
    memset(addr, 0, size);
    ::munmap(addr, size);

    return buffer;
}

int loadBuffer_mmap(sp<GraphicBuffer> buffer, int platform, const char* in_file, int allow_larger_size=0)
{
    ALOGD("%s enter", __func__);
    int32_t fd     = bufferGetFd(buffer, platform);
    int32_t size   = bufferGetSize(buffer);
    int width      = buffer->getWidth();
    int height     = buffer->getHeight();
    int stride     = buffer->getStride();
    int format     = buffer->getPixelFormat();
    uint64_t usage = buffer->getUsage();
    ALOGD("%s fd=%d, size=%d, in_file=%s", __func__, fd, size, in_file);
    ALOGD("%s w=%d, h=%d, s=%d, f=%d, u=0x%" PRIx64, __func__,
        width, height, stride, format, usage);

    if (fd <0 || size <=0 || !in_file) {
        ALOGE("%s error condition: fd=%d, size=%d, in_file=%s", __func__, fd, size, in_file);
        return -1;
    }

    // 读取文件
    int file_size = fileGetSize(in_file);
    if (allow_larger_size == 0 && file_size > size) {
        ALOGE("%s file size %d > buffer size %d", __func__, file_size, size);
        return -1;
    }

    uint8_t* pixels = (uint8_t*)malloc(file_size);
    if (pixels == NULL) {
        ALOGE("%s malloc fail!", __func__);
        return -1;
    }
    FILE* fp = fopen(in_file, "rb");
    if (fp == NULL) {
        ALOGE("cannot open file");
        return -1;
    }
    size_t read_len = fread(pixels, sizeof(uint8_t), file_size, fp);
    if (read_len != file_size) {
        ALOGE("read num %d != image size: %d", (int)read_len, file_size);
        fclose(fp);
        if (pixels != NULL)
            free(pixels);
        return -1;
    }
    fclose(fp);

    // 写buffer
    void* addr = ::mmap(nullptr, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        ALOGE("mmap failed");
        if (pixels != NULL)
            free(pixels);
        return -1;
    }
    // 清空buffer
    memset(addr, 0, size);
    if (file_size < size)
        memcpy(addr, pixels, file_size);
    else
        memcpy(addr, pixels, size);
    ::munmap(addr, size);
    if (pixels != NULL)
        free(pixels);

    return 0;
}

int loadBuffer_lock(const sp<GraphicBuffer>& buffer, uint32_t rgba,
        int platform, const char* in_file, int allow_larger_size=0) {
    ALOGD("%s enter, RGBA color=0x%x", __func__, rgba);

    uint8_t r = rgba & 0xff;
    uint8_t g = (rgba >> 8) & 0xff;
    uint8_t b = (rgba >> 16) & 0xff;
    uint8_t a = (rgba >> 24) & 0xff;

    if(buffer->getWidth() <= 0 || buffer->getHeight() <= 0) {
        ALOGE("%s Impossible buffer size!", __func__);
        return -1;
    }
    int width = static_cast<int32_t>(buffer->getWidth());
    int height = static_cast<int32_t>(buffer->getHeight());

    void* buffer_data{nullptr};
    int32_t stride_in_bytes = 0;
    if (auto status = buffer->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, &buffer_data,
                                   nullptr /*outBytesPerPixel*/, &stride_in_bytes);
        status < 0) {
        ALOGE("%s Failed to lock buffer_data!", __func__);
        return -1;
    } else {
        ALOGD("%s stride in bytes=%d", __func__, stride_in_bytes);
    }
    int32_t stride = (int)buffer->getStride();

    if (in_file != NULL) {
        // 读取文件
        int file_size = fileGetSize(in_file);
        if (file_size < width * height * 4) {
            ALOGE("%s file size %d < buffer size %d", __func__, file_size, width * height * 4);
            return -1;
        }

        uint8_t* pixels = (uint8_t*)malloc(file_size);
        if (pixels == NULL) {
            ALOGE("%s malloc fail!", __func__);
            return -1;
        }
        FILE* fp = fopen(in_file, "rb");
        if (fp == NULL) {
            ALOGE("cannot open file");
            return -1;
        }
        size_t read_len = fread(pixels, sizeof(uint8_t), file_size, fp);
        if (read_len != file_size) {
            ALOGE("read num %d != image size: %d", (int)read_len, file_size);
            fclose(fp);
            if (pixels != NULL)
                free(pixels);
            return -1;
        }
        fclose(fp);

        uint8_t* data = pixels;
        for (int32_t j = 0; j < height; j++) {
            uint8_t* iter = (uint8_t*)buffer_data + (stride * j) * 4;
            for (int32_t i = 0; i < width; i++) {
                iter[0] = data[0];
                iter[1] = data[1];
                iter[2] = data[2];
                iter[3] = data[3];
                iter += 4;
                data += 4;
            }
        }

        free(pixels);
    } else {
        for (int32_t j = 0; j < height; j++) {
            uint8_t* iter = (uint8_t*)buffer_data + (stride * j) * 4;
            for (int32_t i = 0; i < width; i++) {
                iter[0] = r;
                iter[1] = g;
                iter[2] = b;
                iter[3] = a;
                iter += 4;
            }
        }
    }

    if (auto status = buffer->unlock(); status < 0) {
        ALOGE("Failed to unlock pixels!");
        return -1;
    }

    return 0;
}

int saveBuffer_mmap(sp<GraphicBuffer> buffer, int platform, const char* out_dir) {
    ALOGD("%s enter", __func__);
    int32_t fd     = bufferGetFd(buffer, platform);
    int32_t size   = bufferGetSize(buffer);
    int width      = buffer->getWidth();
    int height     = buffer->getHeight();
    int stride     = buffer->getStride();
    int format     = buffer->getPixelFormat();
    uint64_t usage = buffer->getUsage();
    ALOGD("%s fd=%d, size=%d", __func__, fd, size);
    ALOGD("%s w=%d, h=%d, s=%d, f=%d, u=0x%" PRIx64, __func__,
        width, height, stride, format, usage);

    if (fd <0 || size <=0 || !out_dir) {
        ALOGE("error condition: fd=%d, size=%d, out_dir=%s", fd, size, out_dir);
        return -1;
    }

    // 生成文件名字
    char dump_file[200];
    snprintf(dump_file, 200, "%s/IM_f%d_s%d_w%d_h%d_u0x%" PRIx64 ".raw",
            out_dir, format, stride, width, height, usage);

    // mmap dump
    ALOGD("%s mmap dump to %s", __func__, dump_file);
    void* addr = ::mmap(nullptr, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        ALOGE("mmap failed");
        return -1;
    }
    FILE* fp = fopen(dump_file, "wb");
    if (fp == NULL) {
        ALOGE("cannot open file: %s", dump_file);
        return -1;
    }
    fwrite(addr, size, sizeof(char), fp);
    fclose(fp);
    ::munmap(addr, size);

    return 0;
}

int saveBuffer_lock(const sp<GraphicBuffer>& buffer, const char* filename) {
    ALOGD("%s enter, filename=%s", __func__, filename);

    void* pixels{nullptr};
    int32_t stride{0};
    if (auto status = buffer->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, &pixels,
                                   nullptr /*outBytesPerPixel*/, &stride);
        status < 0) {
        ALOGE("Failed to lock pixels!");
        return -1;
    }
    stride = (int)buffer->getStride();
    int width = (int)buffer->getWidth();
    int height = (int)buffer->getHeight();
    int format = (int)buffer->getPixelFormat();
    uint64_t usage = buffer->getUsage();
    ALOGD("%s buffer info:", __func__);
    ALOGD("%s     width=%d", __func__, width);
    ALOGD("%s     height=%d", __func__, height);
    ALOGD("%s     stride=%d", __func__, stride);
    ALOGD("%s     format=0x%x", __func__, format);
    ALOGD("%s     usage=%" PRIx64, __func__, usage);

    int im_size = height * stride * sizeof(uint32_t);
    FILE* fp = fopen(filename, "wb");
    if (fp != NULL) {
        fwrite(pixels, im_size, 1, fp);
        fclose(fp);
    } else {
        ALOGE("file cannot open: %s", filename);
    }

    if (auto status = buffer->unlock(); status < 0) {
        ALOGE("Failed to unlock pixels!");
        return -1;
    }

    return 0;
}
