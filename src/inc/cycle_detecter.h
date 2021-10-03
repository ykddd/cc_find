//
// Created by ykddd on 2021/9/26.
//

#ifndef CYCLE_DETECT_CYCLE_DETECTER_H
#define CYCLE_DETECT_CYCLE_DETECTER_H

#include "transfer.h"
#include "perf_utils.h"
#include "flat_hash_map.h"
#include "tbb/atomic.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <unordered_map>
#include <array>

class CycleDetecter {
public:
    CycleDetecter(char *account_file, char *trans_file, char* res_file) :
            account_file(account_file), trans_file(trans_file), output_file(res_file) {
        PerfThis("CycleDetecter construciton");
        auto account_fd = open(account_file, O_RDONLY);
        assert(account_fd >= 0);
        account_file_len = lseek(account_fd, 0, SEEK_END);
        account_data = (char *) mmap(nullptr, account_file_len, PROT_READ, MAP_PRIVATE, account_fd, 0);

        auto trans_fd = open(trans_file, O_RDONLY);
        assert(trans_fd >= 0);
        trans_file_len = lseek(trans_fd, 0, SEEK_END);
        trans_data = (char *) mmap(nullptr, trans_file_len, PROT_READ, MAP_PRIVATE, trans_fd, 0);
    }

    void Run();

private:
    void SortTransfer();

    void FlattenTrans();

    void MakeEdgeIndex();

    void FindCycle();

    void native_six_for(uint32_t cur_node);

    void ExportRes(const std::array<uint32_t, 6> &res_index,
                   const uint32_t cycle_len,
                   char *tmp_buffer);

    void ExportResImpl(uint32_t idx, char *tmp_buffer, uint32_t &buf_idx);

    void ExportResToFile();


private:
    char *account_file = nullptr;
    char *trans_file = nullptr;
    char *output_file = nullptr;

    char *account_data = nullptr;
    char *trans_data = nullptr;

    off_t account_file_len = 0;
    off_t trans_file_len = 0;


    uint32_t edge_num = 0;
    uint32_t account_num = 0;

    uint64_t *account = nullptr;

    Transfer *forward_trans = nullptr;
    Transfer *backward_trans = nullptr;

    uint32_t *forward_src = nullptr;
    uint32_t *forward_dst = nullptr;
    uint32_t *forward_value = nullptr;
    int64_t *forward_ts = nullptr;


    uint32_t *forward_edge_index = nullptr;


    tbb::atomic<uint32_t> cycyle_num{0};

    char *res_data = nullptr;
    tbb::atomic<off_t> res_buff_offset{0};

    ska::flat_hash_map<uint64_t, uint32_t> id_map;
};

#endif //CYCLE_DETECT_CYCLE_DETECTER_H
