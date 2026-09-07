#ifndef COMPANY_DMABUF_TOOL_H
#define COMPANY_DMABUF_TOOL_H

#include <inttypes.h>
#include <map>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <thread>
#include "dma_buf_copy.h"

namespace company {

class CompanyDmabufTool {
public:
    explicit CompanyDmabufTool(const char* dev_path);
    virtual ~CompanyDmabufTool();

    bool isOpen();

    // DMA-BUF 相关操作
    int32_t dmabuf_get_size(int dmabuf_fd);
    int32_t dmabuf_dump_to_file(int dmabuf_fd, const char* filename);
    int32_t dmabuf_copy(int dmabuf_fd_src, int dmabuf_fd_dst);
protected:
    int32_t ic_lock();
    int32_t ic_unlock();

    int32_t active_open_ic_fd(const char* dev_path);
    void active_close_ic_fd();

    int32_t IOCTL_entry(int32_t fd, unsigned long cmd, void* data);
    int32_t READ_entry(int32_t fd, void* data, int32_t len);
    int32_t WRITE_entry(int32_t fd, void* data, int32_t len);
    int32_t OPEN_entry(const char* name, int32_t type);
    void    CLOSE_entry(int32_t fid);
    off_t   LSEEK_entry(int fd, off_t offset, int whence);
    void   *MMAP_entry(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
    void    MUNMAP_entry(void *addr, size_t length);

private:
    int32_t ic_fd_ = -1;
    bool    is_ic_available_ = true; // mutex是否已经锁过一次
    pthread_mutex_t ic_mutex_;
};

}  // namespace company

#endif  // COMPANY_DMABUF_TOOL_H