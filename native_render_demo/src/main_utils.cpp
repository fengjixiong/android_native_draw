#include <getopt.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <iostream>
#include <inttypes.h>
#include "main_utils.h"
#include "company_utils.h"
#include "company_framework_helper.h"
#include "company_test.h"
#include "company_lib_main.h"
#include <dirent.h>

#define __CLASS__ "main_utils"

namespace company {

static input_params_t input_params;
static const struct option options[] =
{
    { "log_level",         1, NULL, 'l' },
    { "image_in_dir",      1, NULL, 'i' },
    { "image_out_dir",     1, NULL, 'o' },
    { "image_width",       1, NULL, 'w' },
    { "image_height",      1, NULL, 'h' },
    { "image_ext",         1, NULL, 'e' },
    { "image_format",      1, NULL, 'r' },
    { "image_usage",       1, NULL, 'u' },
    { "frame_time_ms",     1, NULL, 'T' },
    { "color_rgba",        1, NULL, 'c' },
    { "delay_frame",       1, NULL, 'd' },
    { "ic_test_path",      1, NULL, 'f' },
    { "ic_test_mode",      1, NULL, 'm' },
    { "max_frames",        1, NULL, 'F' },
    { "case_manifest",     1, NULL, 'M' },
    { "is_download",       1, NULL, 'D' },
    { "dmabuf_tool_path",  1, NULL, 'b' },
    { "use_dmabuf_tool",   1, NULL, 'B' },
    { "test_flag",         1, NULL, 't' },
    { "use_gpu",           1, NULL, 'g' },
    { "platform_type",     1, NULL, 'p' },
    { "display_x",         1, NULL, 'x' },
    { "display_y",         1, NULL, 'y' },
    { "display_z",         1, NULL, 'z' },
    { "use_lockycbcr",     0, NULL, 'L' },
    { "allow_larger_size", 0, NULL, 'S' },
    { 0,0,0,0 }
};

static void PrintHelp(const char *exe)
{
    fprintf(stdout, "usage: %s [options] \n", exe);
    fprintf(stdout, "options:\n");
    for (int i = 0; options[i].name; ++i) {
        fprintf(stdout, "  -%c --%-20s %s\n", options[i].val, options[i].name, options[i].has_arg ? "args" : "");
    }
    fprintf(stdout, "\n");
}

void DumpParams() {
    printf("params:\n");
    printf("    image_in_dir     = %s\n", input_params.opt_image_in_dir     );
    printf("    image_out_dir    = %s\n", input_params.opt_image_out_dir    );
    printf("    image_format     = %d\n", input_params.opt_image_format     );
    printf("    image_usage      = 0x%" PRIx64 "\n", input_params.opt_image_usage      );
    printf("    image_ext        = %s\n", input_params.opt_image_ext        );
    printf("    image_width      = %d\n", input_params.opt_image_width      );
    printf("    image_height     = %d\n", input_params.opt_image_height     );
    printf("    frame_time_ms    = %d\n", input_params.opt_frame_time_ms    );
    printf("    color_rgba       = 0x%8x\n", input_params.opt_color_rgba       );
    printf("    delay_frame      = %d\n", input_params.opt_delay_frame      );
    printf("    ic_test_path     = %s\n", input_params.opt_ic_test_path     );
    printf("    ic_test_mode     = %d\n", input_params.opt_ic_test_mode     );
    printf("    max_frames       = %d\n", input_params.opt_max_frames       );
    printf("    case_manifest_file = %s\n", input_params.opt_case_manifest_file );
    printf("    is_download_case = %d\n", input_params.opt_is_download_case );
    printf("    dmabuf_tool_path = %s\n", input_params.opt_dmabuf_tool_path );
    printf("    use_dmabuf_tool  = %d\n", input_params.opt_use_dmabuf_tool  );
    printf("    test_flag        = %d\n", input_params.opt_test_flag        );
    printf("    use_gpu          = %d\n", input_params.opt_use_gpu          );
    printf("    platform_type    = %d\n", input_params.opt_platform_type    );
    printf("    display_x        = %d\n", input_params.opt_display_x        );
    printf("    display_y        = %d\n", input_params.opt_display_y        );
    printf("    display_z        = %d\n", input_params.opt_display_z        );
    printf("    use_lockycbcr    = %d\n", input_params.opt_use_lockycbcr    );
    printf("    allow_larger_size= %d\n", input_params.opt_allow_larger_size);
}

const input_params_t* ParseArgs(int argc, const char** argv)
{
    input_params.is_params_valid = false;
    // 初始化默认参数
    if (true) {
        input_params.opt_image_in_dir     = NULL;
        input_params.opt_image_out_dir    = NULL;
        input_params.opt_image_format     = COMPANY_HAL_PIXEL_FORMAT_RGBA_8888;
        input_params.opt_image_usage      = 0;
        input_params.opt_image_ext        = ".raw";
        input_params.opt_image_width      = 256; //1440;
        input_params.opt_image_height     = 256; //3168;
        input_params.opt_frame_time_ms    = 1000; // 默认1秒
        input_params.opt_color_rgba       = 0x000000ff; // 默认黑色
        input_params.opt_delay_frame      = 1;
        input_params.opt_ic_test_path     = NULL;// 芯片驱动/dev节点
        input_params.opt_ic_test_mode     = 0;   // 芯片测试模式
        input_params.opt_max_frames       = 0;  // 播放N帧自动停止，如果是0则取num_image
        input_params.opt_case_manifest_file = NULL; // case配置文件
        input_params.opt_is_download_case = 0;   // 默认只是验证一下case，不下载到芯片，设置1则下载
        input_params.opt_dmabuf_tool_path = NULL; // DMA-BUF tool 设备地址，比如/dev/dmabuf_copy
        input_params.opt_use_dmabuf_tool  = 0;   // 默认不使用 DMA-BUF tool
        input_params.opt_test_flag        = COMPANY_TEST_MODE_IMAGES;  // 测试方式
        input_params.opt_use_gpu          = 0;  // 使用GPU
        input_params.opt_platform_type    = 0; // 高通-0, MTK-1
        input_params.opt_display_x        = 0; // 显示的左上角.x
        input_params.opt_display_y        = 0; // 显示的左上角.y
        input_params.opt_display_z        = 100001; // z-order
        input_params.opt_use_lockycbcr    = 0; // YUV NV12使用lockYCbCr赋值
        input_params.opt_allow_larger_size= 0; // 允许文件比buffer更大
    }

    // 参数太少
    if (argc < 2) {
        PrintHelp(argv[0]);
        return NULL;
    }

    // 选项变量
    int opt;
    while (-1 != (opt = getopt_long(argc, (char**)argv, "i:o:e:r:u:w:h:t:T:c:d:l:f:m:F:M:D:b:B:g:p:x:y:z:LS", options, NULL))) {
        switch (opt) {
            case 'l': g_log_level                       = atoi(optarg);  break;
            case 'i': input_params.opt_image_in_dir     =      optarg ;  break;
            case 'o': input_params.opt_image_out_dir    =      optarg ;  break;
            case 'e': input_params.opt_image_ext        =      optarg ;  break;
            case 'r': input_params.opt_image_format     = atoi(optarg);  break;
            case 'u': input_params.opt_image_usage      = std::stoull(optarg, nullptr, 0); break;
            case 'w': input_params.opt_image_width      = atoi(optarg);  break;
            case 'h': input_params.opt_image_height     = atoi(optarg);  break;
            case 'T': input_params.opt_frame_time_ms    = atoi(optarg);  break;
            case 'c': input_params.opt_color_rgba       = strtol(optarg, nullptr, 16); break;
            case 'd': input_params.opt_delay_frame      = atoi(optarg);  break;
            case 'f': input_params.opt_ic_test_path     =      optarg ;  break;
            case 'm': input_params.opt_ic_test_mode     = atoi(optarg);  break;
            case 'F': input_params.opt_max_frames       = atoi(optarg);  break;
            case 'M': input_params.opt_case_manifest_file =    optarg ;  break;
            case 'D': input_params.opt_is_download_case = atoi(optarg);  break;
            case 'b': input_params.opt_dmabuf_tool_path =      optarg ;  break;
            case 'B': input_params.opt_use_dmabuf_tool  = atoi(optarg);  break;
            case 't': input_params.opt_test_flag        = atoi(optarg);  break;
            case 'g': input_params.opt_use_gpu          = atoi(optarg);  break;
            case 'p': input_params.opt_platform_type    = atoi(optarg);  break;
            case 'x': input_params.opt_display_x        = atoi(optarg);  break;
            case 'y': input_params.opt_display_y        = atoi(optarg);  break;
            case 'z': input_params.opt_display_z        = atoi(optarg);  break;
            case 'L': input_params.opt_use_lockycbcr    = 1;  break;
            case 'S': input_params.opt_allow_larger_size= 1;  break;
            default:
                PrintHelp(argv[0]);
                printf("error option: %c\n", opt);
                return NULL;
        }
    }

    input_params.is_params_valid = true;
    DumpParams();

    return &input_params;
}

const input_params_t* GetInputArgs()
{
    if (input_params.is_params_valid == false)
        return NULL;
    return &input_params;
}

int ReadRawFile(const char* image_fullpath, uint32_t image_size, uint8_t* image_buffer)
{
    JLOGD("enter, file=%s", image_fullpath);
    FILE* fp = fopen(image_fullpath, "rb");
    if (fp == NULL) {
        JLOGE("cannot open file");
        return -1;
    }
    size_t read_len = fread(image_buffer, sizeof(uint8_t), image_size, fp);
    if (read_len != image_size) {
        JLOGE("read num %d != image size: %d", read_len, image_size);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

int SaveRawFile(const char* image_fullpath, uint32_t image_size, const uint8_t* image_buffer)
{
    JLOGD("enter, file=%s", image_fullpath);
    if (image_buffer == NULL) {
        JLOGE("buffer is NULL");
        return -1;
    }
    FILE* fp = fopen(image_fullpath, "wb");
    if (fp == NULL) {
        JLOGE("cannot open file");
        return -1;
    }
    size_t save_len = fwrite(image_buffer, sizeof(uint8_t), image_size, fp);
    if (save_len != image_size) {
        JLOGE("save num %d != image size: %d", save_len, image_size);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

uint32_t GetFileSize(const char* filepath) {
    FILE* file = fopen(filepath, "rb");
    if (file == nullptr) return -1;

    fseek(file, 0, SEEK_END);
    uint32_t size = (uint32_t)ftell(file);
    fseek(file, 0, SEEK_SET);
    fclose(file);

    return size;
}

bool FileEndsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) {
        return false;
    }
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

std::vector<std::string> ListDir(std::string directoryPath, std::string suffix)
{
    std::vector<std::string> files;
    if (directoryPath == "")
        return files;

    DIR* dir;
    struct dirent* ent;

    if ((dir = opendir(directoryPath.c_str())) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            std::string filename = ent->d_name;

            // 跳过 "." 和 ".." 目录
            if (FileEndsWith(filename, suffix)) {
                files.push_back(filename);
            }
        }
        closedir(dir);
    }
    return files;
}

int LoadImages(const char* image_dir, const char* image_ext, uint8_t** image_buffers, uint32_t* image_size) {
    JLOGD("enter, dir=%s", image_dir);
    *image_size = 0; // 初始化为0
    if (image_dir == NULL) {
        *image_buffers = NULL;
        return 0;
    }

    std::vector<std::string> image_files = ListDir(image_dir, image_ext); // image_ext=*.raw
    int num_images = image_files.size();
    JLOGD("find %d raw files in %s", num_images, image_dir);
    if (num_images == 0) {
        *image_buffers = NULL;
        return 0;
    }

    // 这里要提前检查文件大小，不能依靠width*height
    std::vector<uint32_t> image_sizes;
    image_sizes.reserve(num_images);
    for (int i = 0; i < num_images; i++) {
        std::string image_fullpath = std::string(image_dir) + "/" + image_files[i];
        image_sizes.push_back(GetFileSize(image_fullpath.c_str()));
    }
    uint32_t size = image_sizes[0];
    for (int i = 1; i < num_images; i++) {
        if (size != image_sizes[i]) {
            JLOGE("image file size not same between [0] and [%d]!", i);
            return 0;
        }
    }

    uint8_t* buffers = (uint8_t*)malloc(size * num_images);
    if (buffers == NULL) {
        JLOGE("malloc fail!");
        return 0;
    }
    for (int i = 0; i < num_images; i++) {
        uint8_t* image_buffer = buffers + i * size;
        std::string image_fullpath = std::string(image_dir) + "/" + image_files[i];
        int ret = ReadRawFile(image_fullpath.c_str(), size, image_buffer);
        if (ret != 0) {
            free(buffers);
            buffers = NULL;
            return 0;
        }
    }
    *image_buffers = buffers;
    *image_size    = size;
    JLOGD("exit successfully");
    return num_images;
}

const char* GetPixelTypeName(int type) {
    switch((company_pixel_format_t)type) {
        case COMPANY_HAL_PIXEL_FORMAT_RGBA_8888: return "RGBA8888";
        case COMPANY_HAL_PIXEL_FORMAT_RGBX_8888: return "RGBX8888";
        case COMPANY_HAL_PIXEL_FORMAT_RGB_888: return "RGB888";
        case COMPANY_HAL_PIXEL_FORMAT_RGB_565: return "RGB565";
        case COMPANY_HAL_PIXEL_FORMAT_BGRA_8888: return "BGRA8888";

        case COMPANY_HAL_PIXEL_FORMAT_MTK_RGBA_5551: return "RGBA5551";
        case COMPANY_HAL_PIXEL_FORMAT_MTK_RGBA_4444: return "RGBA4444";

        case COMPANY_HAL_PIXEL_FORMAT_YCBCR_422_SP: return "YCBCR422SP";
        case COMPANY_HAL_PIXEL_FORMAT_YCRCB_420_SP: return "YCBCR420SP";
        case COMPANY_HAL_PIXEL_FORMAT_YCBCR_422_I: return "YCBCR422I";
        case COMPANY_HAL_PIXEL_FORMAT_RGBA_FP16: return "RGBAFP16";
        case COMPANY_HAL_PIXEL_FORMAT_RAW16: return "RAW16";
        case COMPANY_HAL_PIXEL_FORMAT_BLOB: return "BLOB";
        case COMPANY_HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED: return "IMDE";
        case COMPANY_HAL_PIXEL_FORMAT_YCBCR_420_888: return "YCBCR420888";
        case COMPANY_HAL_PIXEL_FORMAT_RAW_OPAQUE: return "OPAQUE";
        case COMPANY_HAL_PIXEL_FORMAT_RAW10: return "RAW10";
        case COMPANY_HAL_PIXEL_FORMAT_RAW12: return "RAW12";
        case COMPANY_HAL_PIXEL_FORMAT_RGBA_1010102: return "RGBA1010102";

        case COMPANY_HAL_PIXEL_FORMAT_YCBCR_P010: return "YUV420P010";
        case COMPANY_HAL_PIXEL_FORMAT_YCbCr_420_SP: return "YUV420NV12";
        default: return "UnKnown";
    }
}

} // namepace company
