#ifndef __UTILS_H
#define __UTILS_H

#include <thread>
#include <vector>
#include <memory>
#include <limits>       // 用于 std::numeric_limits
#include <string>       // 用于 std::string
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>  // 用于 off_t 等类型
#include <android/log.h> // 用于 __android_log_print

namespace company {

void picasso_log_internal(const char *tag, const char *class_, const char *func, int level, const char *fmt, ...);

#define u8__CLASS__ ""
#define CONCAT(x) (const char*)u8##x
#define CONCAT2(x) CONCAT(x)
#define JLOGV(fmt, args...) picasso_log_internal(LOG_TAG, CONCAT2(__CLASS__), __func__, ANDROID_LOG_VERBOSE, fmt, ## args)
#define JLOGD(fmt, args...) picasso_log_internal(LOG_TAG, CONCAT2(__CLASS__), __func__, ANDROID_LOG_DEBUG, fmt, ## args)
#define JLOGI(fmt, args...) picasso_log_internal(LOG_TAG, CONCAT2(__CLASS__), __func__, ANDROID_LOG_INFO, fmt, ## args)
#define JLOGE(fmt, args...) picasso_log_internal(LOG_TAG, CONCAT2(__CLASS__), __func__, ANDROID_LOG_ERROR, fmt, ## args)

bool endsWith(const std::string& str, const std::string& suffix);
std::vector<std::string> listDir(std::string directoryPath, std::string suffix);

extern int g_log_level;
#define MIN(a,b) ((a<b)?(a):(b))

std::vector<std::string> split_string(const std::string& str, char delimiter);
std::string string_trim_space(const std::string& str);
std::string string_trim_backslash(const std::string& str);
std::string extract_filename_from_path(const std::string& file_path);
std::string extract_dir_from_path(const std::string file_path);
std::string get_string_line_valid_content(std::string& str);
bool file_exist(const char *file_name);
bool dir_exist(const char *dir);
bool string_ends_with(const char *str, const char *suffix);
bool string_starts_with(const char *str, const char *prefix);
bool string_ends_with(const std::string& str, const std::string& suffix);
bool string_starts_with(const std::string& str, const std::string& prefix);
bool isHexDigit(char c);
bool isHexString(const std::string& s);
uint32_t convert_str_2_uint32(const char *value_str, bool is_hex);
uint32_t convert_str_2_uint32(const char *value_str);
uint32_t convert_str_2_uint32(std::string& value_str);
uint32_t convert_str_2_uint32(std::string& value_str, bool is_hex);

void draw_info_layer_buffer(uint8_t* buffer, int32_t frame, int32_t screen_height, int32_t screen_width, uint32_t text_color, uint32_t bg_color);

uint32_t compute_crc32(uint32_t *buffer, uint32_t buffer_len);
uint32_t compare_buffer(uint8_t* buffer1, uint32_t bufferSize1, uint8_t* buffer2, uint32_t bufferSize2);

}

#endif
