#ifndef COMPANY_TEST_H
#define COMPANY_TEST_H

#include "main_utils.h"
#include "company_lib_main.h"
// 定义一些测试例
enum {
    COMPANY_TEST_MODE_NONE = 0,

    COMPANY_TEST_MODE_DOWNLOAD_CASE,   // 下载case
    COMPANY_TEST_MODE_IC_SELF_TEST,    // 芯片自测
    COMPANY_TEST_MODE_IMAGES,     // 显示图像文件
};

namespace company {

int dump_buffer(const input_params_t* params, CompanyLibMain* core, CompanyFrameworkHelper* framework, buffer_handle buffer, int frame_index = 0);

int test_download_case(const input_params_t* params, CompanyLibMain* core, CompanyFrameworkHelper* framework);
int test_ic_self_test(const input_params_t* params, CompanyLibMain* core, CompanyFrameworkHelper* framework);
int test_show_images(const input_params_t* params,
    CompanyLibMain* core, CompanyFrameworkHelper* framework, int num_images, int data_size, const uint8_t* data_buffers);
}

#endif
