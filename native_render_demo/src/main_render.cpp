/*
 * 2025.10.30 created by jxfeng
 * 2025.12.19 add options and show image
 * 2026.01.15 增加驱动处理inbuffer和outbuffer支持
 * 2026.01.20 支持读取和下载case
 */

#include <signal.h>
#include <stdio.h>
#include "company_utils.h"
#include "company_framework_helper.h"
#include "company_lib_main.h"
#include "company_test.h"
#include "main_utils.h"

// test begin
#include <sys/sysmacros.h>  // 新版 glibc 需要（glibc 2.25+）
#include <sys/stat.h>       // fstat 需要
// test end

#define __CLASS__ "main_render"
using namespace android;
using namespace company;


// 全局变量
bool mQuit = false;
void Sighandler(int num)
{
    if (num == SIGINT) {
        JLOGI("SIGINT received, stopping...");
        mQuit = true;
    }
}

int main(int argc, const char** argv)
{
    JLOGI("Starting Native Display Demo 0.3.1");
    int ret = 0;

    // 响应ctrl+C
    // signal(SIGINT, Sighandler);


    // 检查参数
    const input_params_t* params = ParseArgs(argc, argv);
    if (params == NULL) {
        JLOGE("parse args error!");
        return -1;
    }

    // 检查参数
    CompanyLibMain* core = new CompanyLibMain();
    if (core == NULL) {
        JLOGE("create company core error!");
        return -1;
    }
    ret = core->InitParams(params->opt_ic_test_path, params->opt_dmabuf_tool_path);
    if (ret != 0) {
        JLOGE("company core init error!");
        delete core;
        return -1;
    }

    // 加载图像
    uint8_t* image_buffers = NULL;
    uint32_t image_size = 0;
    int image_num = LoadImages(params->opt_image_in_dir, params->opt_image_ext, &image_buffers, &image_size);
    if (image_num == 0 || image_size == 0) {
        JLOGI("no images loaded");
    }

    // 显示初始化
    CompanyFrameworkHelper *framework = CompanyFrameworkHelper::GetInstance();
    if (framework == NULL) {
        JLOGE("create framework error!");
        delete core;
        return -1;
    }

    switch (params->opt_test_flag) {
    case COMPANY_TEST_MODE_DOWNLOAD_CASE:   // 下载case
        ret = test_download_case(params, core, framework);
        break;
    case COMPANY_TEST_MODE_IC_SELF_TEST:    // 芯片自测
        ret = test_ic_self_test(params, core, framework);
        break;
    case COMPANY_TEST_MODE_IMAGES:          // 显示图像文件
        ret = test_show_images(params, core, framework, image_num, image_size, image_buffers);
        break;
    default:
        JLOGE("no such test flag: %d", params->opt_test_flag);
        break;
    }

    delete core;

    // 释放图片内存
    if (image_buffers != NULL) {
        free(image_buffers);
        image_buffers = NULL;
    }

    JLOGD("bye ~~");

    return ret;
}


