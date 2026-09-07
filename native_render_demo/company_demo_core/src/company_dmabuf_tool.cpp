#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <string>
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

#include "company_dmabuf_tool.h"

#define __CLASS__ "dmabuf_tool"
#include "company_utils.h"

namespace company {
CompanyDmabufTool::CompanyDmabufTool(const char* dev_path)
{
    JLOGI("constructor");
    ic_fd_ = -1;
    pthread_mutex_init(&ic_mutex_, NULL);
    is_ic_available_ = true;

    if (dev_path != NULL)
        ic_fd_ = active_open_ic_fd(dev_path);
}

CompanyDmabufTool::~CompanyDmabufTool()
{
    active_close_ic_fd();
    pthread_mutex_destroy(&ic_mutex_);
}

int32_t CompanyDmabufTool::ic_lock()
{
    if (is_ic_available_ != true) {
        JLOGE("ic_lock warning!!");
    }
    is_ic_available_ = false;
    pthread_mutex_lock(&ic_mutex_);
    return 0;
}

int32_t CompanyDmabufTool::ic_unlock()
{
    if (is_ic_available_ != false) {
        JLOGE("ic_unlock warning!");
    }
    pthread_mutex_unlock(&ic_mutex_);
    is_ic_available_ = true;
    return 0;
}

int32_t CompanyDmabufTool::active_open_ic_fd(const char* dev_path) {
    if (ic_fd_ > 0) {
        return ic_fd_; // already open
    }

    ic_fd_ = OPEN_entry(dev_path, O_RDWR);
    if (ic_fd_ <= 0) {
        JLOGE("open picasso ic fd failed: %d, ic_fd_=%d", errno, ic_fd_);
        return ic_fd_;
    }

    return ic_fd_;
}

void CompanyDmabufTool::active_close_ic_fd() {
    if (ic_fd_ > 0) {
        CLOSE_entry(ic_fd_);
    }
    ic_fd_ = -1;
}

bool CompanyDmabufTool::isOpen() {
    return ic_fd_ > 0;
}

// 和驱动交互的 ioctl 统一入口
int32_t CompanyDmabufTool::IOCTL_entry(int32_t fid, unsigned long cmd, void* data)
{

    ic_lock();
    int32_t ret = ::ioctl(fid, cmd, data);
    ic_unlock();

    return ret;
}

int32_t CompanyDmabufTool::READ_entry(int32_t fid, void* data, int32_t len)
{
    ic_lock();
    int32_t ret = ::read(fid, data, len);
    ic_unlock();

    return ret;
}

int32_t CompanyDmabufTool::WRITE_entry(int32_t fid, void* data, int32_t len)
{
    ic_lock();
    int32_t ret = ::write(fid, data, len);
    ic_unlock();

    return ret;
}

int32_t CompanyDmabufTool::OPEN_entry(const char* name, int32_t type)
{
    ic_lock();
    int32_t fid = ::open(name, type);
    ic_unlock();

    return fid;
}

void CompanyDmabufTool::CLOSE_entry(int32_t fid)
{
    JLOGD("enter, fid=%d", fid);

    ic_lock();
    ::close(fid);
    ic_unlock();

}

off_t CompanyDmabufTool::LSEEK_entry(int fid, off_t offset, int whence)
{
    JLOGD("enter, offset=%d, whence=%d", (int)offset, whence);
    ic_lock();
    off_t ret = ::lseek(fid, offset, whence);
    ic_unlock();
    return ret;
}

void *CompanyDmabufTool::MMAP_entry(void *addr, size_t length, int prot, int flags, int fid, off_t offset)
{
    JLOGD("enter");

    ic_lock();
    void* ptr = ::mmap(addr, length, prot, flags, fid, offset);
    ic_unlock();
    return ptr;
}

void CompanyDmabufTool::MUNMAP_entry(void *addr, size_t length)
{
    JLOGD("enter");
    ic_lock();
    ::munmap(addr, length);
    ic_unlock();
}

int32_t CompanyDmabufTool::dmabuf_get_size(int fd) {
    JLOGD("enter, fd=0x%x", fd);

    if (isOpen() == false) {
        JLOGE("device is closed");
        return -1;
    }

    dmabufcopy_data_info_t info;
    info.dma_buf_fd = fd;
    info.size_in_byte = 0;

    ssize_t ret = IOCTL_entry(ic_fd_, DMABUFCOPY_CMD_INFO, &info);
    if (ret != 0) {
        JLOGE("ioctl fail!");
        return -1;
    }
    JLOGD("buffer size = %d", info.size_in_byte);
    return info.size_in_byte;
}

int32_t CompanyDmabufTool::dmabuf_dump_to_file(int fd, const char* filename) {
    JLOGD("enter, fd=0x%x, filename=%s", fd, filename);
    int ret = 0;

    if (isOpen() == false) {
        JLOGE("device is closed");
        return -1;
    }

    dmabufcopy_data_dump_begin_t info_begin;
    dmabufcopy_data_dump_end_t info_end;
    FILE *fp = NULL;
    void *buffer = NULL;

    // 1.获取size，驱动中会将buf的数据copy到临时内存中
    info_begin.dma_buf_fd = fd;
    info_begin.size_in_byte = 0;
    ret = (int)IOCTL_entry(ic_fd_, DMABUFCOPY_CMD_DUMP_BEGIN, &info_begin);
    if (ret != 0) {
        JLOGE("ioctl fail!");
        return -1;
    }
    JLOGD("buffer size = %d", info_begin.size_in_byte);
    if (info_begin.size_in_byte <= 0) {
        JLOGE("buffer size <= 0!");
        return -1;
    }

    // 2.mmap获取内容
    buffer = MMAP_entry(NULL, info_begin.size_in_byte, PROT_READ | PROT_WRITE, MAP_SHARED, ic_fd_, 0);
    if (buffer == MAP_FAILED || buffer == NULL) {
        JLOGE("mmap failed, errno:%d", errno);
        ret = -1;
        goto out_end;
    }

    // 3.保存buffer
    fp = fopen(filename, "wb");
    if (fp == NULL) {
        JLOGD("open file error!");
        ret = -1;
        goto out_unmap;
    }
    fwrite(buffer, info_begin.size_in_byte, sizeof(char), fp);
    fclose(fp);

out_unmap:
    // 4.取消mmap
    if (buffer != NULL) {
        MUNMAP_entry(buffer, info_begin.size_in_byte);
        buffer = NULL;
    }
out_end:
    // 5.释放内存
    info_end.dma_buf_fd = fd;
    IOCTL_entry(ic_fd_, DMABUFCOPY_CMD_DUMP_END, &info_end);
    JLOGD("leave with ret=%d", ret);
    return ret;
}

int32_t CompanyDmabufTool::dmabuf_copy(int fd_src, int fd_dst) {
    JLOGD("enter, src=0x%x, dst=0x%x", fd_src, fd_dst);
    if (isOpen() == false) {
        JLOGE("device is closed");
        return -1;
    }

    dmabufcopy_data_copy_t info;
    info.dma_buf_fd_src = fd_src;
    info.dma_buf_fd_dst = fd_dst;

    ssize_t ret = IOCTL_entry(ic_fd_, DMABUFCOPY_CMD_COPY, &info);
    if (ret != 0) {
        JLOGE("ioctl fail!");
        return -1;
    }
    return 0;
}

} // namespace company
