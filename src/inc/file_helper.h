//
// Created by ykddd on 2021/9/25.
//

#ifndef CYCLE_DETECT_FILE_HELPER_H
#define CYCLE_DETECT_FILE_HELPER_H

#include "transfer.h"
#include "flat_hash_map.h"

#include <fcntl.h>
#include <sys/types.h>
#include <unordered_map>

//using IdMapType = std::unordered_map<uint64_t, uint32_t>;
using IdMapType = ska::flat_hash_map<uint64_t, uint32_t>;

class FileHelper {
public:
    FileHelper() = default;

    ~FileHelper() = default;

    static uint32_t GetAccountNum(char *data, off_t file_len);

    static void GetAccount(char *account_data, uint32_t account_num,
                           uint64_t *account, IdMapType &id_map);

    static uint32_t GetEdgeNum(char *data, off_t file_len);

    static void GetTransferFromFile(const char *trans_data,
                                    off_t file_len,
                                    uint32_t edeg_num,
                                    Transfer *forward,
                                    IdMapType &id_map);


};

#endif //CYCLE_DETECT_FILE_HELPER_H
