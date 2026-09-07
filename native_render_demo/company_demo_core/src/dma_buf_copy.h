#ifndef __DMA_BUF_COPY_H
#define __DMA_BUF_COPY_H

#define DMABUFCOPY_CDEV_NAME    "/dev/dma-buf-copy"
#define DMABUFCOPY_IOC_MAGIC    'a'
#define DMABUFCOPY_IOC_CMD_BASE (0x00U)

#define DMABUFCOPY_IOC_INFO       (0x01U + DMABUFCOPY_IOC_CMD_BASE)
#define DMABUFCOPY_IOC_COPY       (0x02U + DMABUFCOPY_IOC_CMD_BASE)
#define DMABUFCOPY_IOC_DUMP_BEGIN (0x03U + DMABUFCOPY_IOC_CMD_BASE)
#define DMABUFCOPY_IOC_DUMP_END   (0x04U + DMABUFCOPY_IOC_CMD_BASE)

struct dmabufcopy_data_info_t {
    int dma_buf_fd;
    int size_in_byte;
};

struct dmabufcopy_data_copy_t {
    int dma_buf_fd_src;
    int dma_buf_fd_dst;
};

struct dmabufcopy_data_dump_begin_t {
    int dma_buf_fd;
    int size_in_byte;
};

struct dmabufcopy_data_dump_end_t {
    int dma_buf_fd;
};

#define DMABUFCOPY_CMD_INFO       _IOWR(DMABUFCOPY_IOC_MAGIC, DMABUFCOPY_IOC_INFO, struct dmabufcopy_data_info_t)
#define DMABUFCOPY_CMD_COPY       _IOWR(DMABUFCOPY_IOC_MAGIC, DMABUFCOPY_IOC_COPY, struct dmabufcopy_data_copy_t)
#define DMABUFCOPY_CMD_DUMP_BEGIN _IOWR(DMABUFCOPY_IOC_MAGIC, DMABUFCOPY_IOC_DUMP_BEGIN, struct dmabufcopy_data_dump_begin_t)
#define DMABUFCOPY_CMD_DUMP_END   _IOWR(DMABUFCOPY_IOC_MAGIC, DMABUFCOPY_IOC_DUMP_END, struct dmabufcopy_data_dump_end_t)


#endif
