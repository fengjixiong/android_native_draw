#include "company_utils.h"
#include <getopt.h>
#include <sys/stat.h>
#include <math.h>
#include <stdio.h>
#include <dirent.h>
#include <iostream>
#include <sstream>
#include <unistd.h>

#include "digit_160x160.h"

#define __CLASS__ "company_utils"

namespace company {

bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) {
        return false;
    }
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

std::vector<std::string> listDir(std::string directoryPath, std::string suffix)
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
            if (endsWith(filename, suffix)) {
                files.push_back(filename);
            }
        }
        closedir(dir);
    }
    return files;
}

int ReadRawFile(const char* image_fullpath, int image_width, int image_height, const char* image_type, uint8_t* image_buffer)
{
    JLOGD("enter, file=%s", image_fullpath);
    FILE* fp = fopen(image_fullpath, "rb");
    if (fp == NULL) {
        JLOGE("cannot open file");
        return -1;
    }
    int image_size = image_width * image_height * 4;
    size_t read_len = fread(image_buffer, sizeof(uint8_t), image_size, fp);
    if (read_len != image_size) {
        JLOGE("read num %d != image size: %d", read_len, image_size);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

int SaveRawFile(const char* image_fullpath, int image_width, int image_height, const char* image_type, const uint8_t* image_buffer)
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
    int image_size = image_width * image_height * 4;
    size_t save_len = fwrite(image_buffer, sizeof(uint8_t), image_size, fp);
    if (save_len != image_size) {
        JLOGE("save num %d != image size: %d", save_len, image_size);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

void FillRGBA8BufferInColor(uint8_t* bufferData, int o_width, int o_height, int stride, int r, int g, int b)
{
    JLOGV("enter");
    for (int y = 0; y < o_height; y++) {
        for (int x = 0; x < o_width; x++) {
            uint8_t* pixel = bufferData + (4 * (y*stride + x));
            pixel[0] = r;
            pixel[1] = g;
            pixel[2] = b;
            pixel[3] = 255;
        }
    }
    JLOGV("leave");
}

void FillRGBA8BufferInImage(uint8_t* bufferData, int o_width, int o_height, int stride, const uint8_t* bufferImage, int i_width, int i_height)
{
    JLOGV("enter");
    int width  = MIN(i_width,  o_width);
    int height = MIN(i_height, o_height);

    for (int y = 0; y < height; y++) {
        uint8_t* lineBuffer = bufferData  + (4 * y * stride);
        const uint8_t* lineImage = bufferImage + (4 * y * i_width);
        memcpy(lineBuffer, lineImage, 4 * width);
    }
    JLOGV("leave");
}

// 全局变量
const char* log_levels[10] = {"none", "none", "verbose", "debug", "info ", "error", "error" };
int g_log_level = 2; // 默认允许打印的log级别

// JLOGD函数
void picasso_log_internal(const char *tag, const char *class_, const char *func, int level, const char *fmt, ...) {
    if (level < g_log_level)
        return;

    if (level <= ANDROID_LOG_VERBOSE)
        level = ANDROID_LOG_DEBUG;

    va_list args;
    va_start(args, fmt);
    char g_log_buf[2048];
    vsnprintf(g_log_buf, sizeof(g_log_buf), fmt, args);

    // 通过 android log 打印的信息自带时间戳、线程号、级别
    __android_log_print(level, tag, "%s::%s %s", class_, func, g_log_buf);

    // 打印信息到stdout
    fprintf(stdout, "[%s] %s %s::%s %s\n", log_levels[level], tag, class_, func, g_log_buf);

    va_end(args);
}


/*csv中的格式可以是
1. 空行
2. 只有一个逗号
3. 数据行符合后面的格式（16进制）：addr，value
4. 注释只能在行尾
*/

std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::istringstream iss(str);
    std::vector<std::string> result;
    std::string token;
    while (std::getline(iss, token, delimiter)) {
        result.push_back(token);
    }
    return result;
}

std::string string_trim_space(const std::string& str) {
    // 去除左端空白字符
    size_t first = str.find_first_not_of(" \t\n\r\f\v");
    if (first == std::string::npos) {
        return "";  // 字符串全为空白字符
    }

    // 去除右端空白字符
    size_t last = str.find_last_not_of(" \t\n\r\f\v");

    return str.substr(first, (last - first + 1));
}

std::string string_trim_backslash(const std::string& str) {
    // 去除左端空白字符
    size_t pos = str.find("//");
    if ( pos == 0) {
        return " ";
    }
    if (pos != std::string::npos) {
        std::string result = str.substr(0, pos);
        return result;
    }
    return str;
}

std::string extract_filename_from_path(const std::string& file_path) {
    size_t last_slash = file_path.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        return file_path.substr(last_slash + 1);
    } else {
        return file_path;
    }
}

std::string extract_dir_from_path(const std::string file_path) {
    size_t last_slash = file_path.find_last_of("\\/");
    if (last_slash != std::string::npos) {
        return file_path.substr(0, last_slash);
    } else {
        return file_path;
    }
}

void remove_tail_carriage_return(std::string& str) {
    if (!str.empty() && str.back() == '\r') {
        str.pop_back();
    }
}

std::string get_string_line_valid_content(std::string& str) {
    remove_tail_carriage_return(str);
    std::string result = string_trim_backslash(str);
    result = string_trim_space(result);
    return result;
}

bool file_exist(const char *file_name) {
    if (!file_name) {
        return false;
    }

    int32_t result = access(file_name, R_OK);
    if (result != 0) {
        return false;
    }

    struct stat s_buf;
    stat(file_name, &s_buf);
    if (S_ISREG(s_buf.st_mode))
    {
        return true;
    }

    return false;
}

bool dir_exist(const char *dir) {
    if (!dir) {
        return false;
    }

    int32_t result = access(dir, F_OK);
    if (result != 0) {
        return false;
    }

    struct stat s_buf;
    stat(dir, &s_buf);
    if (S_ISDIR(s_buf.st_mode)) {
        return true;
    }

    return false;
}

bool string_ends_with(const char *str, const char *suffix) {
    std::string tmp_str1(str);
    std::string tmp_str2(suffix);
    return string_ends_with(tmp_str1, tmp_str2);
}

bool string_starts_with(const char *str, const char *prefix) {
    std::string tmp_str1(str);
    std::string tmp_str2(prefix);
    return string_starts_with(tmp_str1, tmp_str2);
}

bool string_ends_with(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) {
        return false;  // 如果主字符串长度小于子串长度，直接返回 false
    }
    return str.substr(str.length() - suffix.length()) == suffix;
}

bool string_starts_with(const std::string& str, const std::string& prefix) {
    return str.compare(0, prefix.size(), prefix) == 0;
}

bool isHexDigit(char c) {
    return std::isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool isDecString(const std::string& s) {
    if (s.empty()) return false;

    for (size_t i = 0; i < s.length(); ++i) {
        if (!std::isdigit(s[i])) {
            return false;  // 遇到非十进制字符
        }
    }
    return true;  // 所有字符都是十进制字符
}

bool isHexString(const std::string& s) {
    if (s.empty()) return false;

    bool is_decimal = isDecString(s);

    if( is_decimal ) {
        //dec string, not hex string
        return true;
    }

    // 检查前缀（如果有的话）
    size_t start = 0;
    if (s.length() >= 2 && (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))) {
        start = 2;  // 跳过前缀
    }

    for (size_t i = start; i < s.length(); ++i) {
        if (!isHexDigit(s[i])) {
            return false;  // 遇到非十六进制字符
        }
    }
    return true;  // 所有字符都是十六进制字符
}

uint32_t convert_str_2_uint32(const char *value_str, bool is_hex) {
    std::string input_str(value_str);
    return convert_str_2_uint32(input_str, is_hex);
}

uint32_t convert_str_2_uint32(const char *value_str) {
    std::string input_str(value_str);
    return convert_str_2_uint32(input_str);
}

uint32_t convert_str_2_uint32(std::string& value_str) {
    bool is_hex = !isDecString(value_str);
    return convert_str_2_uint32(value_str, is_hex);
}

uint32_t convert_str_2_uint32(std::string& value_str, bool is_hex) {
    std::istringstream iss(value_str);
    uint32_t num;

    if(is_hex)
        iss >> std::hex >> num;
    else
        iss >> std::dec >> num;
    return num;
}

void draw_info_layer_buffer(uint8_t* buffer, int32_t frame, int32_t screen_height, int32_t screen_width, uint32_t text_color, uint32_t bg_color) {
    // 默认 screen_height=180, screen_width=480
    uint32_t* frameBuffer = (uint32_t*)buffer; // frameBuffer 是 heightxwidthx32bit 的buffer，ABGR 格式
    const int NUMBER_BIT10 = 3;    // 帧号显示位数
    const int HUGE_FONT_SIZE = 160; // 巨大的字体尺寸

    // 拆解每一位
    int frame_numbers[NUMBER_BIT10] = {};
    for (int i = 0; i < NUMBER_BIT10; i++) {
        frame_numbers[NUMBER_BIT10-1-i] = frame % 10;
        frame /= 10;
    }
    // 原来每个数字 bitmask 是一个 byte 对应 8个pixel
    // 所有数字的bitmask -- 展开，一个byte对应1个pixel
    // **** **** ****
    //  0     1   2
    // **** **** ****
    int mask_rows = HUGE_FONT_SIZE;
    int mask_cols = HUGE_FONT_SIZE;
    int mask_rows_all = mask_rows;
    int mask_cols_all = mask_cols * NUMBER_BIT10 * 8;
    uint8_t* bit_mask_all = new uint8_t[mask_rows_all * mask_cols_all];
    memset(bit_mask_all, 0, mask_rows_all * mask_cols_all * sizeof(uint8_t));

    for (int i = 0; i < NUMBER_BIT10; i++) {
        const uint8_t* p_mask = digit_font_160x160_table[frame_numbers[i]];
        for (int j = 0; j < mask_rows; j++) {
            uint8_t* p_mask_all = bit_mask_all + j * mask_cols_all + i * mask_cols;
            memcpy(p_mask_all, p_mask, mask_cols * sizeof(uint8_t));
            p_mask += mask_cols;
        }
    }

    // 根据上述bitmask 绘制 FrameBuffer，mask和FrameBuffer是一对一的
    int32_t render_width = mask_cols_all;
    if (screen_width < render_width)
        render_width = screen_width;
    int32_t render_height = mask_rows_all;
    if (screen_height < render_height)
        render_height = screen_height;

    for (int i = 0; i < render_height; i++) {
        uint32_t* p_render = frameBuffer + i * screen_width;
        uint8_t*  p_mask   = bit_mask_all + i * mask_cols_all;
        for (int j = 0; j < render_width; j++) {
            if (*p_mask == 0) {
                //*p_render = bg_color;
            } else {
                *p_render = text_color;
            }
            p_render++;
            p_mask++;

        }
    }

    delete[] bit_mask_all;
}

uint32_t compute_crc32(uint32_t *buffer, uint32_t buffer_len) {
    uint32_t crc = 0;
    if (buffer == 0 || buffer_len == 0)
        return crc;
    for (int i = 0; i < buffer_len; i++) {
        crc ^= buffer[i];
    }
    return crc;
}

uint32_t compare_buffer(uint8_t* buffer1, uint32_t bufferSize1,
                        uint8_t* buffer2, uint32_t bufferSize2) {
    uint32_t diff_count = 0;
    if (buffer1 == 0 || bufferSize1 == 0)
        return diff_count;
    if (buffer2 == 0 || bufferSize2 == 0)
        return diff_count;

    uint32_t min_size = (bufferSize1 > bufferSize2)?(bufferSize2):(bufferSize1);
    uint32_t max_size = (bufferSize1 > bufferSize2)?(bufferSize1):(bufferSize2);
    diff_count += (max_size - min_size);

    for (int i = 0; i < min_size; i++) {
        if (buffer1[i] != buffer2[i]) {
            diff_count++;
        }
    }
    return diff_count;
}

} // namespace company
