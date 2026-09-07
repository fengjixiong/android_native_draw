#include <errno.h>
#include <unordered_map>
#include <fstream>
#include <limits.h>
// #include <picasso_names.h>
#include <string.h>
#include "company_utils.h"
// #include "picasso_display.h"
// #include <picasso_logger.h>
#include "company_mem_map.h"
// #include "company_builtin_cases.h"

#define __CLASS__ "CompanyMemMap"


// 多少个连续地址的寄存器可以合并为 burst
#define MERGE_THRES 8

namespace company {

CompanyMemMap::CompanyMemMap() {
    int capacity = 512*1024;
    desc_ = (mem_descriptor_t *)malloc(capacity * sizeof(char));
    payload_ = capacity;

    desc_->capacity = capacity;
    desc_->valid_size = capacity;   // 全部 memory 有效，因为 item 和 payload 在头尾分开保存
    desc_->item_capacity = capacity / sizeof(item_header_t); // 不限制 item 个数，中间内存够就可以添加
    desc_->item_valid_cnt = 0;
    desc_->operation = 0;
}
CompanyMemMap::~CompanyMemMap() {
    // if (builtin_cases_map_.size() > 0)
    //     builtin_cases_map_.clear();

    free(desc_);
    desc_ = nullptr;
    payload_ = 0;
}


int CompanyMemMap::get_buffer(uint32_t* buffer, int *buffer_len_in_uint32) {

    if (buffer_len_in_uint32 == NULL || buffer == NULL)
        return -1;

    if (desc_->item_valid_cnt <= 0)
        return -1;

    int buffer_index = 0;
    for (int i = 0; i < desc_->item_valid_cnt; i++) {
        item_header_t *item = &desc_->item_headers[i];
        uint32_t *data = (uint32_t *)((uint8_t*)desc_ + item->offset);

        if (item->payload_len < 4)
            continue;

        if (item->sub_type == CFG_TYPE_SINGLE_WRITE) {
            int item_count = item->payload_len / 4;
            for (int j = 0; j < item_count; j ++) {
                buffer[buffer_index] =  data[j];
                buffer_index++;
            }
        }
        else if (item->sub_type == CFG_TYPE_BULK_WRITE) {
            int item_count = item->payload_len / 4;
            uint32_t addr = data[0];
            for (int j = 1; j < item_count; j++) {
                buffer[buffer_index] =  addr + j * 4 - 4;
                buffer_index++;
                buffer[buffer_index] =  data[j];
                buffer_index++;
            }
        }
        if (buffer_index >= *buffer_len_in_uint32) {
            JLOGE("error index: %d >= %d", buffer_index, *buffer_len_in_uint32);
            return -1;
        }
    }
    *buffer_len_in_uint32 = buffer_index;

    FILE* fp = fopen("mmap_buffer.txt", "wt");
    if (fp == NULL) {
        JLOGE("error open file");
        return -1;
    }
    for (int i = 0; i < buffer_index / 2; i++) {
        fprintf(fp, "%08X,%08X", buffer[2*i], buffer[2*i+1]);
    }
    fclose(fp);

    JLOGD("leave, len=%d", buffer_index);
    return 0;
}


int CompanyMemMap::add_raw(uint32_t type, uint32_t sub_type, const void *payload, uint32_t len_of_uint8, const std::string &name) {
    len_of_uint8 += 3;
    len_of_uint8 &= ~3U;
    if (nullptr == payload || 0 == len_of_uint8) {
        return -EINVAL;
    }

    uint8_t *dst = (uint8_t *)_add_item(type, sub_type, len_of_uint8, name);
    if (nullptr == dst) {
        return -ENOMEM;
    }
    memcpy(dst, payload, len_of_uint8);
    return 0;
}

int CompanyMemMap::add_bin_file(uint32_t addr, const std::string &filename, const std::string &name) {
    std::ifstream file_in(filename, std::ios::binary|std::ios::ate);
    if (!file_in.is_open()) {
        JLOGE("reg cfg manifest file %s open error:%d", filename.c_str(), -errno);
        return -EIO;
    }

    int32_t filesize = (int32_t)file_in.tellg();
    file_in.seekg(0, std::ios::beg);

    if (0 == filesize) {
        file_in.close();
        return -EINVAL;
    }

    uint32_t *dst = (uint32_t *)_add_item(_PICASSO_PQ_PIPE_MISC, CFG_TYPE_BULK_WRITE, sizeof(uint32_t) + filesize, name);
    if (nullptr == dst) {
        file_in.close();
        return -ENOMEM;
    }

    dst[0] = addr;
    file_in.read((char *)&dst[1], filesize);
    file_in.close();

    return 0;
}

int CompanyMemMap::add_csv_file(const std::string &filename, const std::string &name) {
    //JLOGD("enter, file=%s", filename.c_str());
    std::ifstream file_in(filename, std::ios::binary | std::ios::ate);
    if (!file_in.is_open()) {
        JLOGE("reg cfg csv file %s open error:%d", filename.c_str(), -errno);
        return -EIO;
    }
    std::streamsize filesize = file_in.tellg();
    file_in.seekg(0, std::ios::beg);
    std::vector<char> filebuf(filesize);
    file_in.read(filebuf.data(), filesize);
    file_in.close();
    // csv 对应多个 item，失败也要 revert 多个 item
    int ret = 0;
    uint32_t old_item_cnt = desc_->item_valid_cnt;
    uint32_t old_payload = payload_;
    // 从 csv 解析出来的整型数值
    bool in_comment = false;
    int ndigits = 0;
    uint32_t value = 0;

    // 解析 csv，首先把连续的寄存器放入 start_addr,burst_vals
    // 遇到不连续的情况，检查是否足够长，如果不够长则以 single-write 形式输出
    uint32_t start_addr = 0;
    std::vector<uint32_t> burst_vals;
    std::vector<uint32_t> single_writes;
    bool is_addr = true;
    for (char *p = filebuf.data(), *E = filebuf.data() + filebuf.size(); p < E; ++p) {
        if (in_comment) {
            if (*p == '\r' || *p == '\n') {
                in_comment = false;
            }
            continue;
        }

        switch (*p) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            value <<= 4;
            value |= (*p - '0') & 0xf;
            ++ndigits;
            break;
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            value <<= 4;
            value |= (*p - 'a' + 10) & 0xf;
            ++ndigits;
            break;
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            value <<= 4;
            value |= (*p - 'A' + 10) & 0xf;
            ++ndigits;
            break;
        case '/': // 遇到一个斜线就认为是注释
            in_comment = true;
            // fallthrough
        default: // 遇到其他字符都表示分隔
            if (0 == ndigits) {
                break;
            }
            if (is_addr && (value & 3)) {
                JLOGE("parsed address %08x not aligned", value);
            }
            if (!is_addr) {
                burst_vals.push_back(value);
            } else if (value != start_addr + burst_vals.size() * 4) {
                // 地址不再连续，开始一个新的 range
                // 检查当前缓存的 burst-range，如果足够长，则创建一个 burst（还要把之前的 single 输出）
                if (burst_vals.size() >= MERGE_THRES) {
                    ret = _add_single(_PICASSO_PQ_PIPE_MISC, single_writes, name);
                    if (ret) { goto out; }
                    single_writes.clear();
                    ret = _add_burst(_PICASSO_PQ_PIPE_MISC, start_addr, burst_vals, name);
                    if (ret) { goto out; }
                } else {
                    // 不够长，合并到 single_writes 里面
                    for (const uint32_t &i : burst_vals) {
                        single_writes.push_back(start_addr);
                        single_writes.push_back(i);
                        start_addr += 4;
                    }
                }
                start_addr = value;
                burst_vals.clear();
            }
            value = 0;
            ndigits = 0;
            is_addr = !is_addr;
            break;
        }
    }

    if (!is_addr) {
        burst_vals.push_back(value);
    }

    // 这部分逻辑重复，可以将 parser 状态放在类成员变量里维护
    if (burst_vals.size() >= MERGE_THRES) {
        ret = _add_single(_PICASSO_PQ_PIPE_MISC, single_writes, name);
        if (ret) { goto out; }
        ret = _add_burst(_PICASSO_PQ_PIPE_MISC, start_addr, burst_vals, name);
        if (ret) { goto out; }
    } else {
        // 不够长，合并到 single_writes 里面
        for (const uint32_t &i : burst_vals) {
            single_writes.push_back(start_addr);
            single_writes.push_back(i);
            start_addr += 4;
        }
        ret = _add_single(_PICASSO_PQ_PIPE_MISC, single_writes, name);
    }

out:
    if (ret) {
        desc_->item_valid_cnt = old_item_cnt;
        payload_ = old_payload;
    }
    return ret;
}

// TODO 如果 item 非常短，还可以把 item->name 利用起来，不保存名称，而是保存寄存器
//      或者将 item->name 开头四个字节作为字符串，添加 '\0' 防止字符串打印越界
void *CompanyMemMap::_add_item(uint32_t type, uint32_t subtype, uint32_t payload_len, const std::string &name) {
    if (nullptr == desc_) {
        JLOGE("desc_ is null");
        return nullptr;
    }

    item_header_t *item = &desc_->item_headers[desc_->item_valid_cnt];
    uint32_t require = sizeof(item_header_t) + payload_len;
    if ((uint8_t*)item + require > (uint8_t*)desc_ + payload_) {
        return nullptr; // memory not sufficient, cannot add more item!
    }

    desc_->item_valid_cnt++;
    payload_ -= payload_len;

    item->type = type;
    item->sub_type = subtype;
    item->offset = payload_;
    item->payload_len = payload_len;
    snprintf((char *)item->name, ITEM_HEADER_NAME_LEN, "%s", name.c_str());

    return (uint8_t*)desc_ + payload_;

}

// 创建 burst 模式的 item，只要有连续 12 个寄存器，创建 burst item 就能节省空间
int CompanyMemMap::_add_burst(uint32_t type, uint32_t addr, const std::vector<uint32_t> &vals, const std::string &name) {
    if (0 == vals.size()) {
        return 0;
    }


    uint32_t *dst = (uint32_t *)_add_item(type, CFG_TYPE_BULK_WRITE, sizeof(uint32_t) * (vals.size() + 1), name);
    if (nullptr == dst) {
        return -ENOMEM;
    }

    dst[0] = addr;
    memcpy(&dst[1], vals.data(), vals.size() * sizeof(uint32_t));
    return 0;

}

// non-burst
int CompanyMemMap::_add_single(uint32_t type, const std::vector<uint32_t> &buff, const std::string &name) {
    if (buff.size() & 1) {
        JLOGE("non burst data is not paired");
        return -EINVAL;
    }
    if (0 == buff.size() || buff.size() & 1) {
        return 0;
    }


    uint32_t *dst = (uint32_t *)_add_item(type, CFG_TYPE_SINGLE_WRITE, sizeof(uint32_t) * buff.size(), name);
    if (nullptr == dst) {
        return -ENOMEM;
    }

    memcpy(dst, buff.data(), buff.size() * sizeof(uint32_t));
    return 0;
}

int32_t CompanyMemMap::load_manifest_file(const char* manifest_file_name) {

    JLOGD("try load reg manifest file %s", manifest_file_name);

    int32_t load_result = 0;
    if (manifest_file_name == NULL)
        return -1;

    std::ifstream file_in(manifest_file_name);

    if (!file_in.is_open()) {
        JLOGE("reg cfg manifest file %s open error:%d", manifest_file_name, -errno);
        return -1;
    }

    std::string cfg_file_name = extract_filename_from_path(manifest_file_name);

    std::string dir_name = extract_dir_from_path(manifest_file_name);

    std::string line_in = "";
    while (std::getline(file_in, line_in)) {
        line_in = get_string_line_valid_content(line_in);
        std::string header_desc = extract_filename_from_path(line_in);

        if (line_in.length() > 0) {
            if (!string_starts_with(line_in, "/")) {
                // manifest 文件中是相对路径，需要与 manifest 路径合并
                line_in = dir_name + "/" + line_in;
            }

            if (file_exist(line_in.c_str())) {
                if (string_ends_with(line_in, ".csv")) { //如果是csv文件
                    JLOGD("try load csv cfg file:%s", line_in.c_str());
                    load_result = add_csv_file(line_in, header_desc);
                    if (load_result) {
                        JLOGE("load csv file %s error:%d", line_in.c_str(), load_result);
                        break;
                    }

                } else if (string_ends_with(line_in, ".bin")) { //如果是bin文件 ,kernel or code bin
                    JLOGD("try load bin cfg file:%s", line_in.c_str());
                    if (line_in.find("kernel") != std::string::npos) { //kernel bin
                        load_result = add_bin_file(0x280000, line_in, header_desc);
                        if (load_result) {
                            JLOGE("load kernel file %s error:%d", line_in.c_str(), load_result);
                            break;
                        }

                    } else if (line_in.find("code") != std::string::npos) { //code bin
                        // load_result = load_bin_file(line_in, mem_desc, 0x2C0000);
                        load_result = add_bin_file(0x2C0000, line_in, header_desc);
                        if (load_result) {
                            JLOGE("load code file %s error:%d", line_in.c_str(), load_result);
                            break;
                        }
                    } else { //是否可以按照0xXXXXXXX_????.bin 这样的方式解析这个文件名， 如果可以，就将前面的地址部分作为基址加载
                        bool can_load = false;
                        std::vector<std::string> splited_string = split_string(line_in, '_');
                        if (splited_string.size() >= 2) {
                            if (isHexString(splited_string[0])) {
                                JLOGD("try load common brust bin file:%s", line_in.c_str());
                                uint32_t brust_addr = convert_str_2_uint32(splited_string[0], true);
                                load_result = add_bin_file(brust_addr, line_in, header_desc);
                                if (load_result) {
                                    JLOGE("load bin file %s error:%d", line_in.c_str(), load_result);
                                    break;
                                }
                                can_load = true;
                            }
                        }

                        if (!can_load) {
                            JLOGE("can't load bin file:%s, no valid base address", line_in.c_str());
                            load_result = -EPERM;
                            break;
                        }
                    }
                }
            } else {
                load_result = -ENOENT;
                JLOGE("can't find cfg file:%s exit usecase !!!!!!!!!!!!!!!", line_in.c_str());
                break;
            }
        }
        line_in = "";
    }

    file_in.close();

    return load_result;
}


} // namespace company
