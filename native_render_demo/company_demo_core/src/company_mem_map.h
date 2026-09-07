#ifndef COMPANY_MEM_MAP_H
#define COMPANY_MEM_MAP_H

#include <stdint.h>
// #include <picasso_display.h>
#include <string>
#include <vector>
#include <stddef.h>

#define ITEM_HEADER_NAME_LEN 32

typedef struct {
    uint32_t type;          //哪种类型的item
    uint32_t sub_type;      //write mode: single/bulk
    uint32_t payload_len;   //此item有效payload的长度
    uint32_t offset;        //此item距离头的偏移
    uint8_t  name[ITEM_HEADER_NAME_LEN]; //这段寄存器数据的描述
} __attribute__ ((__packed__)) item_header_t;

typedef struct {
    uint32_t capacity;              //这个内存的总容量
    uint32_t valid_size;            //这块内存的有效数据
    uint32_t item_capacity;         //配置头表的总个数
    uint32_t item_valid_cnt;        //配置头表中有效的item个数
    uint32_t operation;             //这个内存是用来做什么操作的 bypassprepare workprepare?
    item_header_t item_headers[0];  //配置头表
} __attribute__ ((__packed__)) mem_descriptor_t;

// 这些 item 里面的 offset 指的是相对于 cfg_regs 数组的偏移量
// 放进 mem_map 时需要加上尾部起始位置
typedef struct usecase_builtin_cfg_item {
    const char          *cfg_key;       // GAME_in_1440x3168_out_1440x3168_rx0-AISR_1-oOOOO_0.0x
    const char          *cfg_dir;       // AP_case50_7_9_1440x3168_PQ_1440x3168
    const char          *cfg_file;      // work_mode-TE030hz_030hz_to_030hz.cfg
    const item_header_t *items;         // 头部数据
    size_t               items_size;
    const uint32_t      *regs;          // 尾部数据
    size_t               regs_size;
} usecase_builtin_cfg_item_t;

typedef struct {
    size_t cfg_item_cnt;
    const usecase_builtin_cfg_item_t **cfg_items;
    const char *case_desc;
} builtin_cfg_item_info_t;

namespace company {


// enum pq_pipe_type {
//     _PICASSO_PQ_PIPE_CSC0R2Y    = BIT(0),
//     _PICASSO_PQ_PIPE_CSC1R2Y    = BIT(1),
//     _PICASSO_PQ_PIPE_CSC2Y2R    = BIT(2),
//     _PICASSO_PQ_PIPE_3DLUT      = BIT(3),
//     _PICASSO_PQ_PIPE_DECHESS    = BIT(4),
//     _PICASSO_PQ_PIPE_SRP        = BIT(5),
//     _PICASSO_PQ_PIPE_LC         = BIT(6),
//     _PICASSO_PQ_PIPE_CGM0       = BIT(7),
//     _PICASSO_PQ_PIPE_CGM1       = BIT(8),
//     _PICASSO_PQ_PIPE_CE         = BIT(9),
//     _PICASSO_PQ_PIPE_COACC      = BIT(10),
//     _PICASSO_PQ_PIPE_DITHER     = BIT(11),
//     _PICASSO_PQ_PIPE_SC1D       = BIT(12),
//     _PICASSO_PQ_PIPE_SC2D       = BIT(13),
//     _PICASSO_PQ_PIPE_UIBLEND    = BIT(14),
//     _PICASSO_PQ_PIPE_MISC       = BIT(31),
// };
#define _PICASSO_PQ_PIPE_MISC (1<<31)

enum cfg_type {
    CFG_TYPE_NULL  = 0,
    CFG_TYPE_SINGLE_WRITE   = 1,
    CFG_TYPE_BULK_WRITE     = 2,
    CFG_TYPE_MCU_WRITE      = 3, /* unused now */
};

// typedef enum {
//     _PICASSO_IP_BLOCK_IMEMC    = BIT(0),
//     _PICASSO_IP_BLOCK_EMEMC    = BIT(1),
//     _PICASSO_IP_BLOCK_AISR     = BIT(2),
//     _PICASSO_IP_BLOCK_VNSS     = BIT(3),
//     _PICASSO_IP_BLOCK_MIPI     = BIT(4),
//     _PICASSO_IP_BLOCK_PQ       = BIT(5),
// }ip_bolck_t;

class CompanyMemMap {
public:
    CompanyMemMap();
    ~CompanyMemMap();

    int load_manifest_file(const char* manifest_file_name);
    int get_buffer(uint32_t* buffer, int *buffer_len_in_uint32);

protected:
    int add_raw(uint32_t type, uint32_t sub_type, const void *payload, uint32_t len_of_uint8, const std::string &name);
    int add_bin_file(uint32_t addr, const std::string &filename, const std::string &name);
    int add_csv_file(const std::string &filename, const std::string &name);
    void *_add_item(uint32_t type, uint32_t sub_type, uint32_t payload_len, const std::string &name);
    int _add_burst(uint32_t type, uint32_t addr, const std::vector<uint32_t> &vals, const std::string &name);
    int _add_single(uint32_t type, const std::vector<uint32_t> &buff, const std::string &name);

    void add_csv_val_(uint32_t val);
    void add_csv_addr_(uint32_t addr);

private:
    mem_descriptor_t *desc_ = nullptr;
    uint32_t payload_ = 0;  // 数据开始地址，从末尾向前增长
    // 解析 csv 过程中保存的状态
    uint32_t curr_type_; //
    std::string curr_name_; // 目前正在解析的文件名

};

} // namespace company

#endif // COMPANY_MEM_MAP_H
