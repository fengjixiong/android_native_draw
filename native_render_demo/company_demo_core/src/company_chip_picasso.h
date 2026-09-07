#ifndef __COMPANY_CHIP_PICASSO_H
#define __COMPANY_CHIP_PICASSO_H


namespace company {

#define PICASSO_DISPLAY_CDEV_NAME           "/dev/picasso-display"
/////////////////////////////  picasso 芯片定义 /////////////////////////
struct picasso_capacity_query {
    bool chip_enable;
    uint32_t vsr_capacity;
    uint32_t aisr_capacity;
    uint32_t memc_capacity;
    uint32_t pq_capacity;
};

struct picasso_device_probe {
    uint16_t chip_id;
    uint8_t  chip_version;
    uint8_t  chip_type;
    uint32_t drv_version; //major[31-24], minor[23-16], patch[15-0]
    uint32_t produce_flag;
    uint8_t  produce_lotid[8];
    uint8_t  produce_waferid[4];
    uint32_t reserved[3];
} __attribute__ ((__packed__));

struct picasso_reg_group_access {
// #define PICASSO_REG_READ    0
// #define PICASSO_REG_WRITE   1
    uint32_t opt;
    uint32_t len;
    uint32_t *buf;
} __attribute__ ((__packed__));

#define PICASSO_IOC_MAGIC                   'p'
#define PICASSO_IOC_CMD_BASE                (0x00U)
#define PICASSO_IOWR(nr, type)              _IOWR(PICASSO_IOC_MAGIC, nr, type)
#define PICASSO_IOC_CAPACITY_QUERY          (0x10U + PICASSO_IOC_CMD_BASE)
#define PICASSO_IOC_DEVICE_PROBE            (0x11U + PICASSO_IOC_CMD_BASE)
#define PICASSO_IOC_REG_GROUP_ACCESS        (0xC2U + PICASSO_IOC_CMD_BASE)
#define PICASSO_DISPLAY_CAPACITY_QUERY PICASSO_IOWR(PICASSO_IOC_CAPACITY_QUERY, struct picasso_capacity_query)
#define PICASSO_DISPLAY_DEVICE_PROBE PICASSO_IOWR(PICASSO_IOC_DEVICE_PROBE, struct picasso_device_probe)
#define PICASSO_DISPLAY_REG_GROUP_ACCESS            0xC01070C2


} // namespace company

#endif
