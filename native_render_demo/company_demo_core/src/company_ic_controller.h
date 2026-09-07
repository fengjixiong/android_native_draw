#ifndef COMPANY_IC_CONTORLLER_H
#define COMPANY_IC_CONTORLLER_H

#include <inttypes.h>
#include <map>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <thread>

namespace company {

class CompanyIcController {
public:
    explicit CompanyIcController(const char* dev_path);
    virtual ~CompanyIcController();
    int32_t init_picasso_ic();

    int32_t IOCTL_entry(int32_t fd, unsigned long cmd, void* data);
    int32_t READ_entry(int32_t fd, void* data, int32_t len);
    int32_t WRITE_entry(int32_t fd, void* data, int32_t len);
    int32_t OPEN_entry(const char* name, int32_t type);
    void    CLOSE_entry(int32_t fid);
    off_t   LSEEK_entry(int fd, off_t offset, int whence);
    void   *MMAP_entry(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
    void    MUNMAP_entry(void *addr, size_t length);

    bool isOpen();

    int32_t self_test_mode(int32_t mode);
    int32_t download_case(const char* manifest_file_name, int is_download);
    int32_t process_grahics_buffer(int32_t in_buffer_fd, int32_t in_width, int32_t in_height, int32_t in_stride, int32_t in_pixel_format,
                                   int32_t out_buffer_fd, int32_t out_width, int32_t out_height, int32_t out_stride, int32_t out_pixel_format);
    int32_t process_grahics_buffer(int32_t srcFd, uint64_t totalSize_in, int32_t dstFd, uint64_t totalSize_out);
protected:
    int32_t ic_lock();
    int32_t ic_unlock();

    int32_t active_open_ic_fd(const char* dev_path);
    void active_close_ic_fd();

private:
    int32_t driver_test_picasso();
    int32_t driver_test_globalmem();
private:
    int32_t ic_fd_ = -1;
    bool    is_ic_available_ = true; // mutex是否已经锁过一次
    pthread_mutex_t ic_mutex_;
};

}  // namespace company

#endif  // COMPANY_IC_CONTORLLER_H