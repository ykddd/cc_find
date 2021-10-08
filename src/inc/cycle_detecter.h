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

struct BackRec {
    BackRec() = default;

    BackRec(uint32_t node, uint32_t b1, uint32_t b2, uint32_t b3) :
            back_node(node), b1_idx(b1), b2_idx(b2), b3_idx(b3) {};

    static bool CmpBackRec(const BackRec &lhs, const BackRec &rhs) {
        return lhs.back_node < rhs.back_node;
    }

    uint32_t back_node;
    uint32_t b1_idx;
    uint32_t b2_idx;
    uint32_t b3_idx;
};

class CycleDetecter {
public:
    CycleDetecter(char *account_file, char *trans_file, char *res_file) :
            account_file(account_file), trans_file(trans_file), output_file(res_file) {
        PerfThis("CycleDetecter construciton");
        auto account_fd = open(account_file, O_RDONLY);
        assert(account_fd >= 0);
        account_file_len = lseek(account_fd, 0, SEEK_END);
        account_data = (char *) mmap(nullptr, account_file_len, PROT_READ, MAP_PRIVATE, account_fd, 0);
        close(account_fd);

        auto trans_fd = open(trans_file, O_RDONLY);
        assert(trans_fd >= 0);
        trans_file_len = lseek(trans_fd, 0, SEEK_END);
        trans_data = (char *) mmap(nullptr, trans_file_len, PROT_READ, MAP_PRIVATE, trans_fd, 0);
        close(trans_fd);
    }

    ~CycleDetecter() {
        munmap(account_data, account_file_len);
        munmap(trans_data, trans_file_len);

        delete[]account;
        delete[]forward_trans;

        delete[]forward_src;
        delete[]forward_dst;
        delete[]forward_value;
        delete[]forward_ts;
        delete[]forward_edge_index;

//        delete []backward_src;
//        delete []backward_dst;
//        delete []backward_value;
//        delete []backward_ts;
//        delete []backward_edge_index;

        delete[]res_data;
    }

    void Run();

private:
    void SortTransfer();

    void SortBackTransfer();

    void FlattenTrans();

    void FlattenBackTrans();

    void MakeEdgeIndex();

    void MakeBackEdgeIndex();

    void FindCycle();

    void NativeSixFor(uint32_t cur_node);

    void BackFindThree(uint32_t cur_node, BackRec *back_rec,
                       bool *in_back, uint32_t *back_record_index, uint32_t &back_recode_num);

    void ForwardFindThree(uint32_t cur_node, BackRec *back_rec, bool *in_back, uint32_t *back_record_index);

    void ExportRes(const std::array<uint32_t, 7> &res_index,
                   uint32_t cycle_len, char *tmp_buffer);

    void ExportResImpl(const std::array<uint32_t, 7> &res_index, char *tmp_buffer,
                       uint32_t &buf_idx, uint32_t cur_idx);

    void ExportResWithBackRec(const std::array<uint32_t, 7> &res_index,
                              uint32_t cycle_len, char *tmp_buffer);

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

    uint32_t *forward_src = nullptr;
    uint32_t *forward_dst = nullptr;
    uint32_t *forward_value = nullptr;
    int64_t *forward_ts = nullptr;
    uint32_t *forward_edge_index = nullptr;

    uint32_t *backward_src = nullptr;
    uint32_t *backward_dst = nullptr;
    uint32_t *backward_value = nullptr;
    int64_t *backward_ts = nullptr;
    uint32_t *backward_edge_index = nullptr;


    uint32_t cycyle_num;
    tbb::atomic<uint32_t> cycyle_num_len_3{0};
    tbb::atomic<uint32_t> cycyle_num_len_4{0};
    tbb::atomic<uint32_t> cycyle_num_len_5{0};
    tbb::atomic<uint32_t> cycyle_num_len_6{0};

    char *res_data = nullptr;
    tbb::atomic<off_t> res_buff_offset{0};

    ska::flat_hash_map<uint64_t, uint32_t> id_map;
};

#endif //CYCLE_DETECT_CYCLE_DETECTER_H
