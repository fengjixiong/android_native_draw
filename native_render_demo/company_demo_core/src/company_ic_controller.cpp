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

#include "company_ic_controller.h"
#include "company_chip_picasso.h"
#include "company_chip_davinci.h"

#define __CLASS__ "ic_controller"
#include "company_utils.h"
#include "company_mem_map.h"

namespace company {
CompanyIcController::CompanyIcController(const char* dev_path)
{
    JLOGI("constructor");
    // core_inst_ = core_inst;
    // ic_init_state_ = -1;
    ic_fd_ = -1;
    // mem_map_ptr = nullptr;
    // memset(&picasso_query_result_, 0, sizeof(picasso_query_result_));
    // memset(&picasso_probe_result_, 0, sizeof(picasso_probe_result_));

    // hw_id_ = 0;

    pthread_mutex_init(&ic_mutex_, NULL);
    is_ic_available_ = true;

    if (dev_path != NULL)
        ic_fd_ = active_open_ic_fd(dev_path);
}

CompanyIcController::~CompanyIcController()
{
    active_close_ic_fd();
    pthread_mutex_destroy(&ic_mutex_);
}

int32_t CompanyIcController::ic_lock()
{
    if (is_ic_available_ != true) {
        JLOGE("ic_lock warning!!");
    }
    is_ic_available_ = false;
    pthread_mutex_lock(&ic_mutex_);
    return 0;
}

int32_t CompanyIcController::ic_unlock()
{
    if (is_ic_available_ != false) {
        JLOGE("ic_unlock warning!");
    }
    pthread_mutex_unlock(&ic_mutex_);
    is_ic_available_ = true;
    return 0;
}

int32_t CompanyIcController::active_open_ic_fd(const char* dev_path) {
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

void CompanyIcController::active_close_ic_fd() {
    if (ic_fd_ > 0) {
        CLOSE_entry(ic_fd_);
    }
    ic_fd_ = -1;
}

bool CompanyIcController::isOpen() {
    return ic_fd_ > 0;
}

// 和驱动交互的 ioctl 统一入口
int32_t CompanyIcController::IOCTL_entry(int32_t fid, unsigned long cmd, void* data)
{

    ic_lock();
    int32_t ret = ::ioctl(fid, cmd, data);
    ic_unlock();

    return ret;
}

int32_t CompanyIcController::READ_entry(int32_t fid, void* data, int32_t len)
{
    ic_lock();
    int32_t ret = ::read(fid, data, len);
    ic_unlock();

    return ret;
}

int32_t CompanyIcController::WRITE_entry(int32_t fid, void* data, int32_t len)
{
    ic_lock();
    int32_t ret = ::write(fid, data, len);
    ic_unlock();

    return ret;
}

int32_t CompanyIcController::OPEN_entry(const char* name, int32_t type)
{
    ic_lock();
    int32_t fid = ::open(name, type);
    ic_unlock();

    return fid;
}

void CompanyIcController::CLOSE_entry(int32_t fid)
{
    JLOGD("enter, fid=%d", fid);

    ic_lock();
    ::close(fid);
    ic_unlock();

}

off_t CompanyIcController::LSEEK_entry(int fd, off_t offset, int whence)
{
    JLOGD("enter, offset=%d, whence=%d", (int)offset, whence);
    ic_lock();
    off_t ret = ::lseek(fd, offset, whence);
    ic_unlock();
    return ret;
}

void *CompanyIcController::MMAP_entry(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    JLOGD("enter");

    ic_lock();
    void* ptr = ::mmap(addr, length, prot, flags, fd, offset);
    ic_unlock();
    return ptr;
}

void CompanyIcController::MUNMAP_entry(void *addr, size_t length)
{
    JLOGD("enter");
    ic_lock();
    ::munmap(addr, length);
    ic_unlock();
}

int32_t CompanyIcController::driver_test_picasso() {
    JLOGD("enter");
    int32_t ret = 0;
    struct picasso_capacity_query picasso_query_result_ = {};
    struct picasso_device_probe   picasso_probe_result_ = {};

    if (ic_fd_ <= 0) {
        ic_fd_ = OPEN_entry(PICASSO_DISPLAY_CDEV_NAME, O_RDWR);
        if (ic_fd_ <= 0) {
            ret = -ENOENT;
            JLOGE("ic fd open failed");
            goto out;
        }
        JLOGD("open picasso OK: %s", PICASSO_DISPLAY_CDEV_NAME);
    }

    ret = IOCTL_entry(ic_fd_, PICASSO_DISPLAY_CAPACITY_QUERY, &picasso_query_result_);
    if (ret) {
        JLOGE("PICASSO_IOC_QUERY, error:%d", ret);
        goto out;
    }

    if (!picasso_query_result_.chip_enable) {
        JLOGE("PICASSO_IOC_QUERY get ic_enable = 0, ic disabled");
        ret = -1;
        goto out;
    } else {
        JLOGI("get ic capability: vsr %d aisr %d memc %d pq %d",
            picasso_query_result_.vsr_capacity,
            picasso_query_result_.aisr_capacity,
            picasso_query_result_.memc_capacity,
            picasso_query_result_.pq_capacity);
    }

    ret = IOCTL_entry(ic_fd_, PICASSO_DISPLAY_DEVICE_PROBE, &picasso_probe_result_);
    if (ret) {
        JLOGE("PICASSO_IOC_PROBE, error:%d", ret);
        goto out;
    }

    JLOGI("chip probe finished: id %x, ver %d type %d",
        picasso_probe_result_.chip_id,
        picasso_probe_result_.chip_version,
        picasso_probe_result_.chip_type);

    active_close_ic_fd();
out:
    return ret;
}

int32_t CompanyIcController::driver_test_globalmem() {
    JLOGD("enter");
    int32_t ret = 0;
    if (ic_fd_ <= 0) {
        ic_fd_ = OPEN_entry("/dev/globalmem", O_RDWR);
        if (ic_fd_ <= 0) {
            ret = -ENOENT;
            JLOGE("ic fd open failed");
            goto out;
        }
    }

    char buffer[20];
    snprintf(buffer, 20, "hello, driver");
    LSEEK_entry(ic_fd_, 0, SEEK_SET);          // 定位到指定位置
    WRITE_entry(ic_fd_, buffer, sizeof(buffer));   // 从该位置读取
    JLOGD("write buffer: %s", buffer);

    memset(buffer, 0, sizeof(buffer));
    LSEEK_entry(ic_fd_, 0, SEEK_SET);          // 定位到指定位置
    READ_entry(ic_fd_, buffer, sizeof(buffer));   // 从该位置读取
    JLOGD("read buffer before clear: %s", buffer);

    memset(buffer, 0, sizeof(buffer));
    IOCTL_entry(ic_fd_, 1, NULL); // 清理驱动缓存
    LSEEK_entry(ic_fd_, 0, SEEK_SET);          // 定位到指定位置
    READ_entry(ic_fd_, buffer, sizeof(buffer));   // 从该位置读取
    JLOGD("read buffer after clear: %s", buffer);
    active_close_ic_fd();
out:
    return ret;
}

// 芯片自测
int32_t CompanyIcController::self_test_mode(int32_t mode)
{
    JLOGD("enter, mode=%d", mode);
    int32_t ret = 0;

    switch (mode) {
    case 1: // picasso 获取 HWID
        ret = driver_test_picasso();
        break;
    case 2: // globalmem 简单读写
        ret = driver_test_globalmem();
        break;
    default:
        break;
    }

    return ret;
}

int32_t CompanyIcController::download_case(const char* manifest_file_name, int is_download)
{
    JLOGD("enter, manifest_file:%s, is_download=%d", manifest_file_name, is_download);
    int ret;
    int buffer_max_size = 1024*1024;
    uint32_t *buffer = (uint32_t*)malloc(buffer_max_size * sizeof(uint32_t));
    if (buffer == NULL){
        JLOGE("malloc error");
        return -1;
    }

    company::CompanyMemMap* map = new company::CompanyMemMap();
    int buffer_actual_size = buffer_max_size;
    ret = map->load_manifest_file(manifest_file_name);
    if (ret != 0) {
        JLOGE("Load error");
        free(buffer);
        delete map;
        return -1;
    }

    ret = map->get_buffer(buffer, &buffer_actual_size);
    if (ret != 0) {
        JLOGE("buffer error!");
        free(buffer);
        delete map;
        return -1;
    }

    if (is_download == 1) {
        // 要下载到芯片
        if(ic_fd_ < 0) {
            JLOGE("device open failed, close fd :%d, errno:%d", ic_fd_, errno);
            free(buffer);
            delete map;
            return -1;
        }

        struct picasso_reg_group_access reg_info;
        reg_info.len = buffer_actual_size;
        reg_info.buf = buffer;
        reg_info.opt = 1;
        ret = IOCTL_entry(ic_fd_, PICASSO_DISPLAY_REG_GROUP_ACCESS, &reg_info);
        if (ret < 0) {
            JLOGE("ioctl failed: ret=%d\n", ret);
        }
    }

    free(buffer);
    delete map;

    return 0;
}

int32_t CompanyIcController::process_grahics_buffer(int32_t srcFd, int32_t in_width, int32_t in_height, int32_t in_stride, int32_t in_pixel_format,
                                   int32_t dstFd, int32_t out_width, int32_t out_height, int32_t out_stride, int32_t out_pixel_format)
{
    JLOGD("enter, in(%d, %d, %d), out(%d, %d, %d)", srcFd, in_height, in_stride, dstFd, out_height, out_stride);
    uint64_t totalSize_in = in_height * in_stride * 4;
    uint64_t totalSize_out = out_height * out_stride * 4;

    int32_t ret = process_grahics_buffer(srcFd, totalSize_in, dstFd, totalSize_out);
    return ret;
}

int32_t CompanyIcController::process_grahics_buffer(int32_t srcFd, uint64_t totalSize_in, int32_t dstFd, uint64_t totalSize_out)
{
    JLOGD("enter, in(%d, %d), out(%d, %d)", srcFd, (uint32_t)totalSize_in, dstFd, (uint32_t)totalSize_out);

    int ret = 0;
    JLOGD("open read device: %s", DEVNAME_R);
    int mFPGAReadFd = OPEN_entry(DEVNAME_R, O_RDWR);
    if (mFPGAReadFd < 0) {
        JLOGE("cannot open read device: %s", DEVNAME_R);
        return -1;
    }

    JLOGD("open write device: %s", DEVNAME_W);
    int mFPGAWriteFd = OPEN_entry(DEVNAME_W, O_RDWR);
    if (mFPGAWriteFd < 0) {
        JLOGE("cannot open write device: %s", DEVNAME_W);
        return -1;
    }

    // 从外部传入
    // int srcFd = getGraphicBufferFd(srcBuffer);
    // int dstFd = getGraphicBufferFd(gameBuffer);

    JLOGD("check buffer fd");
    if (srcFd < 0 || dstFd < 0) {
        JLOGE("srcBuffer or gameBuffer fd invalid srcFd: %d  dstFd: %d", srcFd, dstFd);
        CLOSE_entry(mFPGAReadFd);
        CLOSE_entry(mFPGAWriteFd);
        return -1;
    }

    JLOGD("compute buffer size");
    // GrahpicBufferMapper::get().getAllocationSize(srcFd, &totalSize_in);
    // GrahpicBufferMapper::get().getAllocationSize(dstFd, &totalSize_out);
    // uint64_t totalSize_in  = in_height * in_stride * 4;
    // uint64_t totalSize_out = out_height * out_stride * 4;
    if (totalSize_in == 0 || totalSize_out == 0) {
        JLOGE("in size or out size == 0");
        CLOSE_entry(mFPGAReadFd);
        CLOSE_entry(mFPGAWriteFd);
        return -1;
    }
    if (totalSize_in != totalSize_out) {
        JLOGE("in size != out size");
        CLOSE_entry(mFPGAReadFd);
        CLOSE_entry(mFPGAWriteFd);
        return -1;
    }

    // Write data to FPGA
    JLOGD("write to FPGA begin");
    struct xdma_aperture_ioctl writeIO;
    // writeIO.buffer = (unsigned long) inBuffer;
    writeIO.len = totalSize_in;
    writeIO.ep_addr = 0;
    writeIO.aperture = 2097152;
    writeIO.done = 0UL;
    writeIO.fd = srcFd;
    ssize_t rc = IOCTL_entry(mFPGAWriteFd, IOCTL_XDMA_APERTURE_W, &writeIO);
    if (rc < 0 || writeIO.error) {
        JLOGE("ioctl fail!");
        CLOSE_entry(mFPGAReadFd);
        CLOSE_entry(mFPGAWriteFd);
        return -1;
    }
    JLOGD("write to FPGA end");

    JLOGD("read from FPGA begin");
    long readLen = 0;
    // Read data from FPGA
    struct xdma_aperture_ioctl readIO;
    // readIO.buffer = (unsigned long)outBuffer;
    readIO.len = totalSize_out;
    readIO.ep_addr = 0;
    readIO.aperture = 2097152;
    readIO.done = 0UL;
    readIO.fd = dstFd;
    rc = IOCTL_entry(mFPGAReadFd, IOCTL_XDMA_APERTURE_R, &readIO);
    readLen = readIO.done;
    if (rc < 0 || readIO.error) {
        JLOGE("ioctl fail!");
        CLOSE_entry(mFPGAReadFd);
        CLOSE_entry(mFPGAWriteFd);
        return -1;
    }
    CLOSE_entry(mFPGAReadFd);
    CLOSE_entry(mFPGAWriteFd);

    JLOGD("leave OK");
    return 0;
}

} // namespace company
