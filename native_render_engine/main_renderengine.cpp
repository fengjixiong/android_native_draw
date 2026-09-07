// 2026.4.16 v0.9 created by fengjx
// 2026.4.23 v1.0 compile OK in Qcom & MTK
// 2026.5. 8 v1.1 change parameter
// 2026.6.17 v1.3 支持filesize和buffersize不一致，清空outbuffer，size不对齐的情况，增加EGL方法

#include <log/log.h>
#include <utils/RefBase.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <mutex>
#include <inttypes.h>   // PRIx64
#include <android/native_window.h>
#include <getopt.h>
#include "main_renderengine.h"

// MTK platform, android_vendor_mediatek_proprietary_hardware_gralloc_extra
// #include "ui/gralloc_extra.h"
// #include "graphics_mtk_defs.h"
// #include "gralloc_mtk_defs.h"

#define ENGINE_VERSION "R1.3.10"
using namespace android;

// 打印常用的enum，不同平台不一样
void dispHelp(int argc, char** argv) {
    printf("%s version = %s\n", argv[0], ENGINE_VERSION);
    printf("  -p <platform>         0-Qcom, 1-MTK\n");
    printf("  -w <width>            in buffer width\n");
    printf("  -h <height>           in buffer height\n");
    printf("  -i <in_file_path>     in buffer file\n");
    printf("  -f <in_format>        in buffer pixel format\n");
    printf("  -u <in_usage>         in buffer usage\n");
    printf("  -o <out_dir>          out buffer dir\n");
    printf("  -F <out_format>       out buffer pixel format\n");
    printf("  -U <out_usage>        out buffer usage\n");
    printf("  -S                    flag: allow file larger\n");
    printf("  -L                    flag: load buffer with lock\n");
    printf("  -V                    flag: use Vulkan method (default GL)\n");
    printf("  -e                    flag: use EGL  8bit (default 0)\n");
    printf("  -E                    flag: use EGL 10bit (default 0)\n");

    printf("constants:\n");
    printf("PIXEL_FORMAT_RGBA_8888:        %u\n", (uint32_t)PIXEL_FORMAT_RGBA_8888);
    printf("PIXEL_FORMAT_RGB_888:          %u\n", (uint32_t)PIXEL_FORMAT_RGB_888);
    printf("PIXEL_FORMAT_RGBA_1010102:     %u\n", (uint32_t)PIXEL_FORMAT_RGBA_1010102);
    printf("HAL_PIXEL_FORMAT_YCRCB_420_SP: %u\n", (uint32_t)HAL_PIXEL_FORMAT_YCRCB_420_SP);
    printf("HAL_PIXEL_FORMAT_YCBCR_420_888:%u\n", (uint32_t)HAL_PIXEL_FORMAT_YCBCR_420_888);
    printf("HAL_PIXEL_FORMAT_YCbCr_420_888:%u\n", (uint32_t)HAL_PIXEL_FORMAT_YCbCr_420_888);
    printf("HAL_PIXEL_FORMAT_YCBCR_P010:   %u\n", (uint32_t)HAL_PIXEL_FORMAT_YCBCR_P010);
}

int main(int argc, char** argv) {
    ALOGD("%s enter, version = %s", __func__, ENGINE_VERSION);

    int platform = 0; // 0-Qcom, 1-MTK
    int in_pixel_format          = HAL_PIXEL_FORMAT_RGBA_8888;
    int out_pixel_format         = in_pixel_format;
    int width                    = 256;
    int height                   = 256;
    const char*  in_file_path    = NULL;
    const char*  out_dir         = NULL;
    uint64_t in_usage            = GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_SW_WRITE_OFTEN;
    uint64_t out_usage           = GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE;
    int buffer_load_use_lock     = 0; // raw & in buffer 使用 lock 的方式load
    int allow_larger_size        = 0; // 允许 filesize > buffersize
    int renderengine_VulKan      = 0; // 使用 Vulkan方法（默认 GL方法）
    int use_egl_8bit             = 0; // 使用 chengquansen EGL 8bit 方法
    int use_egl_10bit            = 0; // 使用 chengquansen EGL 10bit 方法

    if (argc == 1) {
        dispHelp(argc, argv);
        exit(-1);
    }
    int c = 0;
    while((c = getopt(argc, argv, "p:w:h:i:o:f:F:u:U:SLVeE")) != -1) {
    switch (c) {
        case 'p': platform = atoi(optarg); break;
        case 'w': width = atoi(optarg); break;
        case 'h': height = atoi(optarg); break;
        case 'i': in_file_path = optarg; break;
        case 'o': out_dir = optarg; break;
        case 'f': in_pixel_format = atoi(optarg); break;
        case 'F': out_pixel_format = atoi(optarg); break;
        case 'u': in_usage = std::stoull(optarg, nullptr, 0); break;
        case 'U': out_usage = std::stoull(optarg, nullptr, 0); break;
        case 'S': allow_larger_size = 1; break;
        case 'L': buffer_load_use_lock = 1; break;
        case 'V': renderengine_VulKan = 1; break;
        case 'e': use_egl_8bit = 1; break;
        case 'E': use_egl_10bit = 1; break;
        default:
            dispHelp(argc, argv);
            exit(-1);
    }}

    printf("%s (version: %s) params:\n", argv[0], ENGINE_VERSION);
    printf("  -p platform         = %d\n",            platform);
    printf("  -w width            = %d\n",            width);
    printf("  -h height           = %d\n",            height);
    printf("  -i in_file_path     = %s\n",            in_file_path);
    printf("  -o out_dir          = %s\n",            out_dir);
    printf("  -f in_pixel_format  = %d\n",            in_pixel_format);
    printf("  -F out_pixel_format = %d\n",            out_pixel_format);
    printf("  -u in_usage         = 0x%" PRIx64 "\n", in_usage);
    printf("  -U out_usage        = 0x%" PRIx64 "\n", out_usage);
    printf("  -S allow_larger_size= %d\n",            allow_larger_size);
    printf("  -L load_use_lock    = %d\n",            buffer_load_use_lock);
    printf("  -V use VK_engine    = %d\n",            renderengine_VulKan);
    printf("  -e use_egl_8bit     = %d\n",            use_egl_8bit);
    printf("  -E use_egl_10bit    = %d\n\n",          use_egl_10bit);

    int ret = 0;

    // 1. 创建in-buffer
    printf("(1/5) create in buffer\n");
    sp<GraphicBuffer> in_buffer = createBufferAndClean(platform, width, height, in_pixel_format, in_usage, "src_buffer");
    if (in_buffer == nullptr) {
        printf("create in buffer error! \n");
        return -1;
    }

    // 2. 创建out-buffer
    printf("(2/5) create out buffer\n");
    sp<GraphicBuffer> out_buffer = createBufferAndClean(platform, width, height, out_pixel_format, out_usage, "dst_buffer");
    if (in_buffer == nullptr) {
        printf("create in buffer error! \n");
        return -1;
    }

    // 3. 加载in-buffer
    printf("(3/5) load in buffer\n");
    if (buffer_load_use_lock && (in_usage & GRALLOC_USAGE_SW_WRITE_OFTEN) != 0)
        ret = loadBuffer_lock(in_buffer, 0xff0000ff, platform, in_file_path, allow_larger_size);
    else
        ret = loadBuffer_mmap(in_buffer, platform, in_file_path, allow_larger_size);
    if (ret != 0) {
        printf("load error!\n");
        return -1;
    }

    // 4. 转换buffer
    printf("(4/5) change in buffer to out buffer\n");
    if (use_egl_8bit == 1 || use_egl_10bit == 1) {
        ret = copyBuffer_gles(in_buffer, out_buffer, use_egl_10bit);
    } else {
        ret = copyBuffer_engine(in_buffer, out_buffer, renderengine_VulKan);
    }
    if (ret != 0) {
        printf("load error!\n");
        return -1;
    }

    // 5. 保存输出buffer
    printf("(5/5) save out buffer\n");
    if (out_dir == NULL)
        ret = saveBuffer_lock(out_buffer, "copybuffer.raw");
    else
        ret = saveBuffer_mmap(out_buffer, platform, out_dir);
    if (ret != 0) {
        printf("load error!\n");
        return -1;
    }

    printf("======== all done OK! ===========\n");
    return ret;
}

