#ifndef __COMPANY_LIB_MAIN__
#define __COMPANY_LIB_MAIN__

#include "company_utils.h"
namespace company {

typedef enum
{
    COMPANY_LIB_MAIN_PROCESS_NONE = 0,
    COMPANY_LIB_MAIN_PROCESS_OK,
    COMPANY_LIB_MAIN_PROCESS_ERROR,
} company_process_result_t;

class CompanyIcController;
class CompanyDmabufTool;

class CompanyLibMain
{
public:
    CompanyLibMain();
    ~CompanyLibMain();
    int32_t InitParams(const char* opt_ic_test_path, const char* opt_dmabuf_tool_path);
    company_process_result_t IcSelfTest(int test_mode);
    company_process_result_t DownloadCase(const char *case_manifest_file, int download_flag);
    int32_t ProcessFrame(int32_t srcFd, uint64_t totalSize_in, int32_t dstFd, uint64_t totalSize_out);

    bool IsDeviceAvailable();

    // DMA-BUF 相关操作
    int32_t DmaBufGetSize(int dmabuf_fd);
    int32_t DmaBufDumpToFile(int dmabuf_fd, const char* filename);
    int32_t DmaBufCopy(int dmabuf_fd_src, int dmabuf_fd_dst);
private:
    CompanyIcController* _ic_controllor = NULL;
    CompanyDmabufTool*   _dmabuf_tool   = NULL;

}; // class CompanyLibMain

} // namespace company


#endif
