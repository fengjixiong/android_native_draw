#ifndef COMPANY_MAIN_UTILS_H
#define COMPANY_MAIN_UTILS_H


namespace company {


typedef struct input_params_tag {
    bool        is_params_valid;
    const char *opt_image_in_dir;
    const char *opt_image_out_dir;
    const char *opt_image_ext;
    int         opt_image_format;
    uint64_t    opt_image_usage;
    int         opt_image_width;
    int         opt_image_height;
    int         opt_frame_time_ms;
    int         opt_color_rgba;
    int         opt_delay_frame;
    const char *opt_ic_test_path;
    int         opt_ic_test_mode;
    int         opt_max_frames;
    const char *opt_case_manifest_file;
    int         opt_is_download_case;
    const char *opt_dmabuf_tool_path;
    int         opt_use_dmabuf_tool;
    int         opt_test_flag;
    int         opt_use_gpu;
    int         opt_platform_type;
    int         opt_display_x;
    int         opt_display_y;
    int         opt_display_z;
    int         opt_use_lockycbcr;
    int         opt_allow_larger_size;
} input_params_t;

const input_params_t* ParseArgs(int argc, const char** argv);
const input_params_t* GetInputArgs();

int ReadRawFile(const char* image_fullpath, int image_size, uint8_t* image_buffer);
int SaveRawFile(const char* image_fullpath, int image_size, const uint8_t* image_buffer);
int LoadImages(const char* image_dir, const char* image_ext, uint8_t** image_buffers, uint32_t* image_size);

const char* GetPixelTypeName(int type);

}
#endif
