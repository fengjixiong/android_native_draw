#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>  // 用于 off_t 等类型
#include <sys/mman.h>
#include <thread>
#include <vector>
#include <memory>
#include <limits>       // 用于 std::numeric_limits
#include <string>       // 用于 std::string
#include <unistd.h>
#include <inttypes.h>  // PRIx64
#include "company_utils.h"
#include "company_framework_helper.h"
#include "company_lib_main.h"
#include "main_utils.h"

#define __CLASS__ "company_test"
using namespace android;

#define MIN(a,b) ((a<b)?(a):(b))
namespace company {

int BufferFillWithImage_MTK_NV12(CompanyFrameworkHelper* helper, buffer_handle buffer,
    const uint8_t* data, int data_size, int platform=0) {
    JLOGD("enter");
    int buffer_size = (int)helper->BufferGetTotalSize(buffer);
    int width = helper->BufferGetWidth(buffer);
    int height = helper->BufferGetHeight(buffer);
    JLOGD("image size (%d), buffer size (%d)", data_size, buffer_size);
    if (buffer_size < data_size) {
        return -1;
    }
    CompanyYCbCr_t ycbcr {};
    CompanyRect_t rect;
    rect.x = 0;
    rect.y = 0;
    rect.width = width;
    rect.height = height;
    int ret = helper->BufferLockYCbCr(buffer, COMPANY_GRALLOC_USAGE_SW_WRITE_OFTEN, rect, &ycbcr);
    if (ret != 0) {
        JLOGE("lockYCbCr error: %d", ret);
        return -1;
    }

    uint8_t* y_dst  = static_cast<uint8_t*>(ycbcr.y);
    uint8_t* cb_dst = static_cast<uint8_t*>(ycbcr.cb);
    uint8_t* cr_dst = static_cast<uint8_t*>(ycbcr.cr);

    const size_t yStride    = ycbcr.ystride;
    const size_t cStride    = ycbcr.cstride;
    const size_t chromaStep = ycbcr.chroma_step;

    JLOGD("YUV420_888 Buffer width=%d Height=%d", width, height);
    JLOGD("YUV420_888 Buffer yStride=%lld, cStride=%lld chromaStep=%lld",
         (long long)yStride, (long long)cStride, (long long)chromaStep);
    JLOGD("YUV420_888 Y Start as:%llx CB Start as:%llx Cr Start as:%llx",
         (long long)y_dst, (long long)cb_dst, (long long)cr_dst);

    // 根据 NV21 结构，推导 NV12 结构
    uint8_t* y_src = (uint8_t*)data;
    uint8_t* cb_src = y_src + (cr_dst - y_dst); // NV12的CB是NV21的CR
    uint8_t* cr_src = y_src + (cb_dst - y_dst); // NV12的CR是NV21的CR

    JLOGD("File Y Start as:%llx CB Start as:%llx Cr Start as:%llx",
         (long long)y_src, (long long)cb_src, (long long)cr_src);

    for (uint32_t y = 0; y < height; y++) {
        uint8_t* yRow_dst = y_dst + y * yStride;
        uint8_t* yRow_src = y_src + y * yStride;
        memcpy(yRow_dst, yRow_src, width * sizeof(uint8_t));
    }

    for (uint32_t y = 0; y < height / 2; y++) {
        for (uint32_t x = 0; x < width / 2; x++) {
            int index = y * cStride + x * chromaStep;
            cb_dst[index] = cb_src[index];
            cr_dst[index] = cr_src[index];
        }
    }

    helper->BufferUnlock(buffer);
    return 0;
}

int BufferFillWithImage(CompanyFrameworkHelper* helper, buffer_handle buffer,
    const uint8_t* data, int data_size, int platform=0, int use_lockycbcr=0)
{
    JLOGD("enter");
    int buffer_format = helper->BufferGetFormat(buffer);
    if (buffer_format == COMPANY_HAL_PIXEL_FORMAT_YCBCR_420_888 &&
        platform == 1 && use_lockycbcr == 1) {// 用NV21表示NV12
        // MTK 平台下，NV12图像无法直接通过mmap，因为没找到合适的NV12格式
        return BufferFillWithImage_MTK_NV12(helper, buffer, data, data_size, platform);
    } else {
        int buffer_fd = helper->BufferGetFd(buffer, platform);
        void* addr = ::mmap(nullptr, data_size, PROT_READ|PROT_WRITE, MAP_SHARED, buffer_fd, 0);
        if (addr == MAP_FAILED) {
            JLOGE("mmap failed");
            return -1;
        }
        memcpy(addr, data, data_size);
        ::munmap(addr, data_size);
    }

    return 0;
}

int BufferFillWithColor(CompanyFrameworkHelper* helper, buffer_handle buffer, int color_rgba, int platform=0) {

    JLOGD("enter, buffer=%d, color=0x%08X", buffer, color_rgba);

    uint32_t r = (color_rgba >> 24) & 0xff;
    uint32_t g = (color_rgba >> 16) & 0xff;
    uint32_t b = (color_rgba >>  8) & 0xff;
    uint32_t a = color_rgba & 0xff;
    uint32_t pixel = r | (g << 8) | (b << 16) | (a << 24);

    int buffer_fd = helper->BufferGetFd(buffer, platform);
    int buffer_size = (int)helper->BufferGetTotalSize(buffer);
    if ((buffer_size % 4) != 0) {
        JLOGE("data-size is not 4-times! %d", buffer_size);
        return -1;
    }
    void* addr = ::mmap(nullptr, buffer_size, PROT_READ|PROT_WRITE, MAP_SHARED, buffer_fd, 0);
    if (addr == MAP_FAILED) {
        JLOGE("mmap failed");
        return -1;
    }
    uint32_t* pixels = (uint32_t*)addr;
    int pixels_len = buffer_size / 4;
    for (int i = 0; i < pixels_len; i++) {
        pixels[i] = pixel;
    }
    ::munmap(addr, buffer_size);
    return 0;
}

int test_download_case(const input_params_t* params,
    CompanyLibMain* core, CompanyFrameworkHelper* framework) {

    JLOGD("enter");
    int ret = core->DownloadCase(params->opt_case_manifest_file, params->opt_is_download_case);
    return ret;
}

int test_ic_self_test(const input_params_t* params,
    CompanyLibMain* core, CompanyFrameworkHelper* framework) {

    JLOGD("enter");
    int ret = core->IcSelfTest(params->opt_ic_test_mode);
    return ret;
}

int dump_buffer(const input_params_t* params, CompanyLibMain* core,
    CompanyFrameworkHelper* framework, buffer_handle buffer, int num_images, int frame_index = 0) {
    int platform         = params->opt_platform_type;
    int i_width          = params->opt_image_width;
    int i_height         = params->opt_image_height;
    int i_format         = params->opt_image_format;

    const char* out_dir  = params->opt_image_out_dir;
    int32_t fd = framework->BufferGetFd(buffer, platform);
    int32_t size = (int32_t)framework->BufferGetTotalSize(buffer);
    JLOGV("fd=%d, size=%d", fd, size);
    JLOGD("dump dir=%s, buffer=%d, frame=%d", out_dir, buffer, frame_index);
    if (fd <0 || size <=0 || !out_dir) {
        JLOGE("error condition: fd=%d, size=%d, out_dir=%s", fd, size, out_dir);
        return -1;
    }

    // uint64_t i_usage     = params->opt_image_usage;
    const char* i_fmt_str= GetPixelTypeName(i_format);

    int o_width          = framework->BufferGetWidth(buffer);
    int o_height         = framework->BufferGetHeight(buffer);
    int o_stride         = framework->BufferGetStride(buffer);
    int o_format         = framework->BufferGetFormat(buffer);
    uint64_t o_usage     = framework->BufferGetUsage(buffer);
    const char* o_fmt_str= GetPixelTypeName(o_format);
    int use_gpu          = params->opt_use_gpu;

    const char* img_ext  = "raw";
    if (use_gpu) {
        if (platform == 0)
            img_ext = "ubwc";
        else if (platform == 1)
            img_ext = "afbc";
    }

    // 生成文件名字
    char dump_file[200];
    if (use_gpu) { // 使用GPU后，format, usage都会变化
        snprintf(dump_file, 200, "%s/IM_%d_%d_%s_s%d_w%d_h%d_%s_W%d_H%d_U0x%" PRIx64 ".%s",
            out_dir, num_images, frame_index,
            i_fmt_str, o_stride, i_width, i_height,
            o_fmt_str, o_width, o_height, o_usage,
            img_ext);
    } else {
        snprintf(dump_file, 200, "%s/IM_%d_%d_%s_s%d_w%d_h%d.%s",
            out_dir, num_images, frame_index,
            i_fmt_str, o_stride, i_width, i_height,
            img_ext);
    }

    // mmap dump
    JLOGD("mmap dump to %s", dump_file);
    void* addr = ::mmap(nullptr, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        JLOGE("mmap failed");
        return -1;
    }
    FILE* fp = fopen(dump_file, "wb");
    if (fp == NULL) {
        JLOGE("cannot open file: %s", dump_file);
        return -1;
    }
    fwrite(addr, size, sizeof(char), fp);
    fclose(fp);
    ::munmap(addr, size);

    return 0;
}

// MTK平台压缩buffer格式
//    pixel_format=2 (COMPANY_HAL_PIXEL_FORMAT_RGBX_8888), RGBA8888->RGBX8888
//    usage=0x40000000000300
//          COMPANY_GRALLOC_USAGE_HW_TEXTURE | COMPANY_GRALLOC_USAGE_HW_RENDER | (1<<54)
// Qcom平台压缩buffer格式
//    pixel_format=2 (COMPANY_HAL_PIXEL_FORMAT_RGBX_8888), RGBA8888->RGBX8888
//    usage=0x00000010000300
//          COMPANY_GRALLOC_USAGE_HW_TEXTURE | COMPANY_GRALLOC_USAGE_QTI_ALLOC_UBWC

uint64_t generate_buffer_usage(uint64_t usage, int platform, int use_gpu) {
    JLOGD("enter, platform=%d, use_gpu=%d", platform, use_gpu);
    if (usage == 0) {
        if (use_gpu == 0) {
            usage = COMPANY_GRALLOC_USAGE_HW_RENDER | COMPANY_GRALLOC_USAGE_SW_WRITE_OFTEN | COMPANY_GRALLOC_USAGE_SW_READ_OFTEN;
        } else {
            if (platform == 0) { // Qcom
                usage = COMPANY_GRALLOC_USAGE_HW_TEXTURE | COMPANY_GRALLOC_USAGE_QTI_ALLOC_UBWC;
            } else if (platform == 1) { // MTK
                uint64_t var_one = 1;
                usage = COMPANY_GRALLOC_USAGE_HW_TEXTURE | COMPANY_GRALLOC_USAGE_HW_RENDER | (var_one<<54);
            }
        }
    }

    return usage;
}

int test_show_images(const input_params_t* params,
    CompanyLibMain* core, CompanyFrameworkHelper* framework, int num_images, int file_size, const uint8_t* data_buffers) {

    JLOGD("enter");
    int color_rgba      = params->opt_color_rgba;
    int max_frame       = params->opt_max_frames;
    int dump_method     = params->opt_use_dmabuf_tool;
    int image_width     = params->opt_image_width;
    int image_height    = params->opt_image_height;
    int sleep_ms        = params->opt_frame_time_ms;
    int pixel_format    = params->opt_image_format;
    uint64_t image_usage= params->opt_image_usage;
    int platform        = params->opt_platform_type;
    const char* img_dir = params->opt_image_in_dir;
    int display_x       = params->opt_display_x;
    int display_y       = params->opt_display_y;
    int display_z       = params->opt_display_z;
    int use_gpu         = params->opt_use_gpu;
    int use_lockycbcr   = params->opt_use_lockycbcr;
    int allow_larger    = params->opt_allow_larger_size;

    // 检查参数
    image_usage = generate_buffer_usage(image_usage, platform, use_gpu);
    if (img_dir == NULL || num_images == 0) {
        JLOGD("no images loaded in image dir %s, show color instead", img_dir);
    }
    if (max_frame == 0 && num_images > 0) {
        JLOGD("max framd changed to num_images: %d", num_images);
        max_frame = num_images;
    }
    if (image_width <= 0)
        image_width = 200;
    if (image_height <= 0)
        image_height = 200;
    if (max_frame <= 0)
        max_frame = 1;

    // 创建 Surface
    surface_handle surface = framework->SurfaceCreate(
        "NDKTest", image_width, image_height, (company_pixel_format_t)pixel_format,
        0, display_x, display_y, display_z
    );
    if (surface == INVALID_HANDLE) {
        JLOGE("create surface fail!");
        return -1;
    }

    // 创建 Buffer
    buffer_handle buffer = framework->BufferCreate(
        "NDKBuffer", image_width, image_height,
        (company_pixel_format_t)pixel_format, image_usage
    );
    if (buffer == INVALID_HANDLE) {
        JLOGE("create buffer fail!");
        framework->SurfaceDelete(surface);
        return -1;
    }

    // 检查 buffer 大小
    int fd         = framework->BufferGetFd(buffer, platform);
    int height     = framework->BufferGetHeight(buffer);
    int stride     = framework->BufferGetStride(buffer);
    int width      = framework->BufferGetWidth(buffer);
    int buffer_size = (int)framework->BufferGetTotalSize(buffer);
    JLOGD("buffer[%d] fd=%d, w=%d, h=%d, s=%d, size=%d", buffer, fd, width, height, stride, buffer_size);

    framework->BufferInfo(buffer, platform);
    if (num_images > 0 && buffer_size < file_size) {
        if (allow_larger == 0) {
            JLOGE("error: buffer size (%d) < file size(%d)!", buffer_size, file_size);
            framework->BufferDelete(buffer);
            framework->SurfaceDelete(surface);
            return -1;
        } else {
            JLOGE("warning: buffer size (%d) < file size(%d), will crop file size", buffer_size, file_size);
        }

    } else {
        JLOGD("buffer[%d] size=%d, filesize=%d", buffer, buffer_size, file_size);
    }

    // 填充纯色
    BufferFillWithColor(framework, buffer, color_rgba, platform);

    // 逐帧显示
    for (int frame_index = 0; frame_index < max_frame; frame_index++) {
        JLOGV("frame [%d] start", frame_index);

        // 暂时不考虑可用性
        // if (0 == framework->bufferAvailable(buffer)) {
        //     JLOGE("buffer %d not available!", buffer);
        //     break;
        // }

        // 填充图像
        if (num_images > 0) {
            const uint8_t* data_buffer = data_buffers + (frame_index % num_images) * file_size;
            BufferFillWithImage(framework, buffer, data_buffer, MIN(file_size, buffer_size), platform, use_lockycbcr);
        }

        // 显示
        framework->SurfaceSetBuffer(surface, buffer);

        // 导出
        if (dump_method > 0) {
            dump_buffer(params, core, framework, buffer, num_images, frame_index);
        }

        // delay模拟帧率
        usleep(1000 * sleep_ms);
    }

    // 释放
    framework->BufferDelete(buffer);
    framework->SurfaceDelete(surface);

    return 0;
}

} // namespace company
