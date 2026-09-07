#ifndef __COMPANY_CHIP_DAVINCI_H
#define __COMPANY_CHIP_DAVINCI_H

#include <sys/ioctl.h>

namespace company {

#define DEVNAME_R "/dev/xdma0_c2h_0"
#define DEVNAME_W "/dev/xdma0_h2c_0"

struct xdma_aperture_ioctl {
    uint64_t ep_addr;
    unsigned int aperture;
    unsigned long buffer;
    unsigned long len;
    int error;
    unsigned long done;
    int fd;
};

#define IOCTL_XDMA_APERTURE_R   _IOW('q', 7, struct xdma_aperture_ioctl *)
#define IOCTL_XDMA_APERTURE_W   _IOW('q', 8, struct xdma_aperture_ioctl *)

}


#endif
