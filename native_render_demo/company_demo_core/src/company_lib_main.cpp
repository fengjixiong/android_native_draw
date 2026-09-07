/*
 * 2025.10.30 created by jxfeng
 * 2025.12.19 add options and show image
 * 2026.01.15 增加驱动处理inbuffer和outbuffer支持
 * 2026.01.20 支持读取和下载case
 * 2026.03.27 将Framework封装，主程序ndk-build
 */
#include <signal.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>  // 用于 off_t 等类型

#include <thread>
#include <vector>
#include <memory>
#include <limits>       // 用于 std::numeric_limits
#include <string>       // 用于 std::string

#include "company_utils.h"
#include "company_ic_controller.h"
#include "company_lib_main.h"
#include "company_dmabuf_tool.h"

#define __CLASS__ "CompanyLibMain"
namespace company {

CompanyLibMain::CompanyLibMain() {}


CompanyLibMain::~CompanyLibMain()
{
    JLOGD("enter");

    // 释放DMA-tool驱动
    if (_dmabuf_tool != NULL) {
        delete _dmabuf_tool;
        _dmabuf_tool = NULL;
    }

    // 释放芯片
    if (_ic_controllor != NULL) {
        delete _ic_controllor;
        _ic_controllor = NULL;
    }

    JLOGD("bye ~~");

}

int32_t CompanyLibMain::InitParams(const char* opt_ic_test_path, const char* opt_dmabuf_tool_path)
{
    int ret = 0;

    // 打开驱动
    if (_ic_controllor != NULL) {
        delete _ic_controllor;
        _ic_controllor = NULL;
    }
    if (opt_ic_test_path != NULL) {
        JLOGD("open IC path: %s", opt_ic_test_path);
        _ic_controllor = new CompanyIcController(opt_ic_test_path);
    } else {
        _ic_controllor = new CompanyIcController(NULL);
    }

    // 打开 DMA-BUF 工具
    if (_dmabuf_tool != NULL) {
        delete _dmabuf_tool;
        _dmabuf_tool = NULL;
    }
    if (opt_dmabuf_tool_path != NULL) {
        JLOGD("open DMA-BUF tool path: %s", opt_dmabuf_tool_path);
        _dmabuf_tool = new CompanyDmabufTool(opt_dmabuf_tool_path);
    }

    return 0;
}

company_process_result_t CompanyLibMain::IcSelfTest(int test_mode)
{
    // 只进行芯片测试，不做别的测试
    if (test_mode > 0 && _ic_controllor != NULL) {
        JLOGD("Only test IC in mode %d", test_mode);
        int32_t ret = _ic_controllor->self_test_mode(test_mode); //params->opt_ic_test_mode);
        if (ret == 0)
            return COMPANY_LIB_MAIN_PROCESS_OK;
        else
            return COMPANY_LIB_MAIN_PROCESS_ERROR;
    }
    return COMPANY_LIB_MAIN_PROCESS_NONE;
}

company_process_result_t CompanyLibMain::DownloadCase(const char *case_manifest_file, int download_flag)
{
    // 配置芯片case
    if (case_manifest_file != NULL && _ic_controllor != NULL) {
        JLOGD("config case");
        int32_t ret = _ic_controllor->download_case(case_manifest_file, download_flag); //params->opt_case_manifest_file, params->opt_is_download_case);
        if (ret == 0)
            return COMPANY_LIB_MAIN_PROCESS_OK;
        else
            return COMPANY_LIB_MAIN_PROCESS_ERROR;
    }

    return COMPANY_LIB_MAIN_PROCESS_NONE;
}

int32_t CompanyLibMain::ProcessFrame(int32_t srcFd, uint64_t totalSize_in, int32_t dstFd, uint64_t totalSize_out)
{
    JLOGV("enter");
    if (_ic_controllor == NULL) {
        JLOGV("IC controller is null");
        return -1;
    }
    return _ic_controllor->process_grahics_buffer(srcFd, totalSize_in, dstFd, totalSize_out);
}

bool CompanyLibMain::IsDeviceAvailable()
{
    return _ic_controllor->isOpen();
}

// DMA-BUF 相关操作
int32_t CompanyLibMain::DmaBufGetSize(int dmabuf_fd) {
    JLOGD("enter");
    if (_dmabuf_tool == NULL) {
        JLOGE("dmabuf_tool_path not set");
        return -1;
    }
    return _dmabuf_tool->dmabuf_get_size(dmabuf_fd);
}

int32_t CompanyLibMain::DmaBufDumpToFile(int dmabuf_fd, const char* filename) {
    JLOGD("enter");
    if (_dmabuf_tool == NULL) {
        JLOGE("dmabuf_tool_path not set");
        return -1;
    }
    return _dmabuf_tool->dmabuf_dump_to_file(dmabuf_fd, filename);
}

int32_t CompanyLibMain::DmaBufCopy(int dmabuf_fd_src, int dmabuf_fd_dst) {
    JLOGD("enter");
    if (_dmabuf_tool == NULL) {
        JLOGE("dmabuf_tool_path not set");
        return -1;
    }
    return _dmabuf_tool->dmabuf_copy(dmabuf_fd_src, dmabuf_fd_dst);
}

} // namespace company