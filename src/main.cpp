//
// Created by ykddd on 2021/9/25.
//

#include "ska_sort.h"
#include "tsl/robin_map.h"
#include "perf_utils.h"
#include "flat_hash_map.h"
#include "tbb/parallel_for.h"
#include "tbb/parallel_for_each.h"
#include "tbb/global_control.h"
#include "tbb/cache_aligned_allocator.h"
#include "tbb/scalable_allocator.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/enumerable_thread_specific.h"
#include "tbb/iterators.h"
#include "tbb/parallel_sort.h"
#include "string_utils.h"
//#include "mimalloc.h"


#include <fcntl.h>
#include <set>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <chrono>
#include <numeric>
#include <vector>
#include <thread>
#include <cmath>
#include <array>


constexpr char lf = '\n';
constexpr char zero = '0';
constexpr char comma = ',';
constexpr char dot = '.';
typedef tbb::enumerable_thread_specific<std::vector<uint32_t>> CounterType;

struct Transfer {
    Transfer() = default;


    static bool cmp_upper(const uint32_t x, const Transfer &y) {
        return x <= y.amount * 10;
    }

    static bool cmp_lower(const Transfer &y, const uint32_t x) {
        return y.amount * 10 <= x;
    }

    static bool cmp_back_upper(const uint32_t x, const Transfer &y) {
        return x <= y.amount * 11;
    }

    static bool cmp_back_lower(const Transfer &y, const uint32_t x) {
        return y.amount * 9 <= x;
    }


    static bool cmp_transfer_ts(const Transfer &lhs, const Transfer &rhs) {
        return lhs.time_stamp < rhs.time_stamp;
    }

    struct sort_fun {
        template<typename T1, typename T2>
        bool operator()(T1 lhs, T2 rhs) const {
            return ((std::get<0>(lhs) < std::get<0>(rhs)) ||
                    (std::get<0>(lhs) == std::get<0>(rhs) && std::get<1>(lhs) > std::get<1>(rhs)));
        }
    };

    uint32_t amount;
    uint32_t src_id;
    uint32_t dst_id;
    uint64_t time_stamp;
};

struct BackRec {
    BackRec() = default;

    BackRec(uint32_t node, uint32_t b1, uint32_t b2, uint32_t b3) :
            back_node(node), b1_idx(b1), b2_idx(b2), b3_idx(b3) {};

    void SetRec(uint32_t node, uint32_t b1, uint32_t b2, uint32_t b3) {
        back_node = node;
        b1_idx = b1;
        b2_idx = b2;
        b3_idx = b3;
    }

    uint32_t back_node;
    uint32_t b1_idx;
    uint32_t b2_idx;
    uint32_t b3_idx;
};

uint32_t account_num;
uint32_t edge_num;

//ska::flat_hash_map<uint64_t, uint32_t> id_map;
tsl::robin_map<uint64_t, uint32_t> id_map;

uint64_t *account;
Transfer *forward;

uint32_t *forward_src;
uint32_t *forward_dst;
uint32_t *forward_value;
uint64_t *forward_ts;
uint32_t *forward_edge_index;
uint32_t *forward_prune_index_beg;
uint32_t *forward_prune_index_end;

Transfer *backward;
uint32_t *backward_src;
uint32_t *backward_dst;
uint32_t *backward_value;
uint64_t *backward_ts;
uint32_t *backward_edge_index;
uint32_t *backward_prune_index_beg;
uint32_t *backward_prune_index_end;

uint32_t **account_thread_offset;


tbb::atomic<off_t> res_buff_offset{0};

auto res_data = new char[335620 * 300];

#define TimeNow() std::chrono::steady_clock::now()
#define Duration(beg) std::chrono::duration<double, std::milli>(TimeNow() - beg).count()

#define GET_UINT64(num, cur, deli)             \
    num = 0;                                   \
    for(; *cur != deli; ++cur){                \
        num = (*cur - zero) + 10 * num;        \
    }                                          \
    ++cur;                                     \

#define IfTrueThenDo(cond, ...)      \
    if (cond) {                      \
     __VA_ARGS__;                    \
    }

//#define SkipTs(last_ts, cur_index, node)                      \
//    for (cur_index = forward_edge_index[node];                \
//         cur_index < forward_edge_index[node + 1];            \
//         ++cur_index) {                                       \
//        IfTrueThenDo(                                         \
//            last_ts < forward_ts[cur_index],   \
//            break;                                            \
//        );                                                    \
//}

#define SkipTsBack(last_index, cur_index, node)               \
    for (cur_index = backward_edge_index[node];               \
         cur_index < backward_edge_index[node + 1];           \
         ++cur_index) {                                       \
        IfTrueThenDo(                                         \
            backward_ts[last_index] > backward_ts[cur_index], \
            break;                                            \
        );                                                    \
}

#define SkipAmt(last_bottom, last_top, cur_index)           \
    if (last_bottom > forward_value[cur_index] * 10 ||      \
        last_top < forward_value[cur_index] * 10) {         \
            continue;                                       \
    }

#define SkipAmtBack(last_value, cur_index)             \
    if (backward_value[cur_index] * 9 > last_value ||  \
        backward_value[cur_index] * 11 < last_value) { \
            continue;                                  \
    }

#define SkipDupliNode(cond)       \
    IfTrueThenDo(cond, continue;) \


void ExportResImpl(uint32_t *res_index, char *tmp_buffer,
                   uint32_t &buf_idx, uint32_t cur_idx) {
    auto tmp = res_index[cur_idx + 1];
    tmp_buffer[buf_idx++] = '(';
    if (cur_idx == 0) {
        buf_idx += StrUtils::u64toa_jeaiii(account[res_index[cur_idx]], tmp_buffer + buf_idx);
    } else {
        buf_idx += StrUtils::u64toa_jeaiii(account[forward_dst[res_index[cur_idx]]], tmp_buffer + buf_idx);
    }
    tmp_buffer[buf_idx++] = ')';
    tmp_buffer[buf_idx++] = '-';
    tmp_buffer[buf_idx++] = '[';
    buf_idx += StrUtils::u64toa_jeaiii(forward_ts[tmp], tmp_buffer + buf_idx);
    tmp_buffer[buf_idx++] = ',';
    buf_idx += StrUtils::u64toa_jeaiii(forward_value[tmp] / 100U, tmp_buffer + buf_idx);
    tmp_buffer[buf_idx++] = '.';
    auto x = forward_value[tmp] % 100U;
    if (x < 10) {
        tmp_buffer[buf_idx++] = '0';
    }
    buf_idx += StrUtils::u64toa_jeaiii(x, tmp_buffer + buf_idx);
    tmp_buffer[buf_idx++] = ']';
    tmp_buffer[buf_idx++] = '-';
    tmp_buffer[buf_idx++] = '>';
}

void ExportRes(uint32_t *res_index,
               uint32_t cycle_len, char *tmp_buffer) {
//    PerfThis(__FUNCTION__);
    //(10)-[100,10.50]->(11)-[102,11.52]->(12)-[104,11.54]->(10)
    uint32_t buf_idx = 0;
    for (size_t i = 0; i < cycle_len; ++i) {
        ExportResImpl(res_index, tmp_buffer, buf_idx, i);
    }
    tmp_buffer[buf_idx++] = '(';
    buf_idx += StrUtils::u64toa_jeaiii(account[res_index[0]], tmp_buffer + buf_idx);
    tmp_buffer[buf_idx++] = ')';
    tmp_buffer[buf_idx++] = '\n';
    off_t cur_res_offset = res_buff_offset.fetch_and_add(buf_idx);
    memcpy(res_data + cur_res_offset, tmp_buffer, buf_idx);
}


void ExportResWithBackRec(uint32_t *res_index,
                          uint32_t cycle_len, char *tmp_buffer) {
//    PerfThis(__FUNCTION__);
    uint32_t buf_idx = 0;
    for (size_t i = 0; i < cycle_len - 3; ++i) {
        auto tmp = res_index[i + 1];
        tmp_buffer[buf_idx++] = '(';
        if (i == 0) {
            buf_idx += StrUtils::u64toa_jeaiii(account[res_index[i]], tmp_buffer + buf_idx);
        } else {
            buf_idx += StrUtils::u64toa_jeaiii(account[forward_dst[res_index[i]]], tmp_buffer + buf_idx);
        }
        tmp_buffer[buf_idx++] = ')';
        tmp_buffer[buf_idx++] = '-';
        tmp_buffer[buf_idx++] = '[';
        buf_idx += StrUtils::u64toa_jeaiii(forward_ts[tmp], tmp_buffer + buf_idx);
        tmp_buffer[buf_idx++] = ',';
        buf_idx += StrUtils::u64toa_jeaiii(forward_value[tmp] / 100U, tmp_buffer + buf_idx);
        tmp_buffer[buf_idx++] = '.';
        auto x = forward_value[tmp] % 100U;
        if (x < 10) {
            tmp_buffer[buf_idx++] = '0';
        }
        buf_idx += StrUtils::u64toa_jeaiii(x, tmp_buffer + buf_idx);
        tmp_buffer[buf_idx++] = ']';
        tmp_buffer[buf_idx++] = '-';
        tmp_buffer[buf_idx++] = '>';
    }
    for (size_t i = cycle_len - 3; i < cycle_len; ++i) {
        auto tmp = res_index[i + 1];
        tmp_buffer[buf_idx++] = '(';
        if (i == cycle_len - 3) {
            buf_idx += StrUtils::u64toa_jeaiii(account[forward_dst[res_index[i]]], tmp_buffer + buf_idx);
        } else {
            buf_idx += StrUtils::u64toa_jeaiii(account[backward_dst[tmp]], tmp_buffer + buf_idx);
        }
        tmp_buffer[buf_idx++] = ')';
        tmp_buffer[buf_idx++] = '-';
        tmp_buffer[buf_idx++] = '[';
        buf_idx += StrUtils::u64toa_jeaiii(backward_ts[tmp], tmp_buffer + buf_idx);
        tmp_buffer[buf_idx++] = ',';
        buf_idx += StrUtils::u64toa_jeaiii(backward_value[tmp] / 100U, tmp_buffer + buf_idx);
        tmp_buffer[buf_idx++] = '.';
        auto x = backward_value[tmp] % 100U;
        if (x < 10) {
            tmp_buffer[buf_idx++] = '0';
        }
        buf_idx += StrUtils::u64toa_jeaiii(x, tmp_buffer + buf_idx);
        tmp_buffer[buf_idx++] = ']';
        tmp_buffer[buf_idx++] = '-';
        tmp_buffer[buf_idx++] = '>';
    }
    tmp_buffer[buf_idx++] = '(';
    buf_idx += StrUtils::u64toa_jeaiii(account[res_index[0]], tmp_buffer + buf_idx);
    tmp_buffer[buf_idx++] = ')';
    tmp_buffer[buf_idx++] = '\n';
    off_t cur_res_offset = res_buff_offset.fetch_and_add(buf_idx);
    memcpy(res_data + cur_res_offset, tmp_buffer, buf_idx);
}

void BackFindThree(uint32_t cur_node, BackRec *back_rec,
                   bool *in_back, uint32_t &back_recode_num) {
    uint32_t node_1, node_2, node_3;
    uint32_t back_amt_1, back_amt_2;
    uint32_t j, k;
    for (uint32_t i = 0; i < back_recode_num; ++i) {
        in_back[back_rec[i].back_node] = false;
    }
    back_recode_num = 0;
    for (uint32_t i = backward_edge_index[cur_node]; i < backward_edge_index[cur_node + 1]; ++i) {
        node_1 = backward_dst[i];
//        back_amt_1 = backward_value[i] * 10;
        for (j = backward_prune_index_beg[i]; j < backward_prune_index_end[i]; ++j) {
            if (backward_ts[j] > backward_ts[i]) {
                continue;
            }
//            SkipAmtBack(back_amt_1, j);
            node_2 = backward_dst[j];
            SkipDupliNode(node_2 == cur_node);
//            back_amt_2 = backward_value[j] * 10;
            for (k = backward_prune_index_beg[j]; k < backward_prune_index_end[j]; ++k) {
                if (backward_ts[k] > backward_ts[j]) {
                    continue;
                }
//                SkipAmtBack(back_amt_2, k);
                node_3 = backward_dst[k];
                SkipDupliNode(node_3 == node_1);
                back_rec[back_recode_num++].SetRec(node_3, k, j, i);
                in_back[node_3] = true;
            }
        }
    }
}

void ForwardFindThree(uint32_t cur_node, BackRec *back_rec,
                      bool *in_back, char *local_res,
                      uint32_t back_recode_num,
                      CounterType::reference counter) {
    uint32_t node_1, node_2, node_3;
    uint32_t back2, back3;
    uint32_t j, k, m;
    uint32_t forward_amt_1_top, forward_amt_2_top, forward_amt_3_top;
    uint32_t forward_amt_1_bottom, forward_amt_2_bottom, forward_amt_3_bottom;
    uint64_t forward_ts_1, forward_ts_2, forward_ts_3;
    auto res_index = new uint32_t[7];
    res_index[0] = cur_node;
    for (uint32_t i = forward_edge_index[cur_node]; i < forward_edge_index[cur_node + 1]; ++i) {
        node_1 = forward_dst[i];
        forward_ts_1 = forward_ts[i];
        forward_amt_1_top = forward_value[i] * 11;
        forward_amt_1_bottom = forward_value[i] * 9;
        if (in_back[node_1]) {
            res_index[1] = i;
            for (j = 0; j < back_recode_num; ++j) {
                BackRec &tmp_back = back_rec[j];
                if (tmp_back.back_node != node_1 ||
                    forward_ts_1 > backward_ts[tmp_back.b1_idx] ||
                    forward_amt_1_bottom > backward_value[tmp_back.b1_idx] * 10 ||
                    forward_amt_1_top < backward_value[tmp_back.b1_idx] * 10) {
                    continue;
                }
                res_index[2] = tmp_back.b1_idx;
                res_index[3] = tmp_back.b2_idx;
                res_index[4] = tmp_back.b3_idx;
                ExportResWithBackRec(res_index, 4, local_res);
                ++counter[1];
            }
        }
        for (j = forward_prune_index_beg[i]; j < forward_prune_index_end[i]; ++j) {
            if (forward_ts[j] < forward_ts_1) {
                continue;
            }
            node_2 = forward_dst[j];
            if (node_2 == cur_node) {
                continue;
            }
            forward_ts_2 = forward_ts[j];
            forward_amt_2_top = forward_value[j] * 11;
            forward_amt_2_bottom = forward_value[j] * 9;
            if (in_back[node_2]) {
                res_index[1] = i;
                res_index[2] = j;
                for (k = 0; k < back_recode_num; ++k) {
                    BackRec &tmp_back = back_rec[k];
                    if (tmp_back.back_node != node_2 ||
                        forward_ts_2 > backward_ts[tmp_back.b1_idx] ||
                        forward_amt_2_bottom > backward_value[tmp_back.b1_idx] * 10 ||
                        forward_amt_2_top < backward_value[tmp_back.b1_idx] * 10 ||
                        backward_dst[tmp_back.b2_idx] == node_1 ||
                        backward_dst[tmp_back.b3_idx] == node_1) {
                        continue;
                    }
                    res_index[3] = tmp_back.b1_idx;
                    res_index[4] = tmp_back.b2_idx;
                    res_index[5] = tmp_back.b3_idx;
                    ExportResWithBackRec(res_index, 5, local_res);
                    ++counter[2];
                }
            }

            for (k = forward_prune_index_beg[j]; k < forward_prune_index_end[j]; ++k) {
                if (forward_ts[k] < forward_ts_2) {
                    continue;
                }
                node_3 = forward_dst[k];
                if (!in_back[node_3] || node_3 == node_1) { continue; }

                IfTrueThenDo(node_3 == cur_node,
                             res_index[1] = i;
                                     res_index[2] = j;
                                     res_index[3] = k;
                                     ExportRes(res_index, 3, local_res);
                                     ++counter[0];
                                     continue;)
                forward_ts_3 = forward_ts[k];
                forward_amt_3_top = forward_value[k] * 11;
                forward_amt_3_bottom = forward_value[k] * 9;
                res_index[1] = i;
                res_index[2] = j;
                res_index[3] = k;
                for (m = 0; m < back_recode_num; ++m) {
                    BackRec &tmp_back = back_rec[m];
                    if (tmp_back.back_node != node_3 ||
                        forward_ts_3 > backward_ts[tmp_back.b1_idx] ||
                        forward_amt_3_bottom > backward_value[tmp_back.b1_idx] * 10 ||
                        forward_amt_3_top < backward_value[tmp_back.b1_idx] * 10 ||
                        backward_dst[tmp_back.b2_idx] == node_1 ||
                        backward_dst[tmp_back.b3_idx] == node_1 ||
                        backward_dst[tmp_back.b2_idx] == node_2 ||
                        backward_dst[tmp_back.b3_idx] == node_2) {
                        continue;
                    }
                    res_index[4] = tmp_back.b1_idx;
                    res_index[5] = tmp_back.b2_idx;
                    res_index[6] = tmp_back.b3_idx;
                    ExportResWithBackRec(res_index, 6, local_res);
                    ++counter[3];
                }
            }
        }
    }
}


int main(int argc, char *argv[]) {
    PerfThis("Total cost");
    auto time_begin = TimeNow();
    auto thread_num = tbb::task_scheduler_init::default_num_threads();
//
//    auto thread_num = 1;
    tbb::global_control c(tbb::global_control::max_allowed_parallelism, thread_num);
    std::cout << thread_num << std::endl;

    char res_file[] = "./result.csv";
//    char trans_file[] = "/Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/data/test/test.csv";
//    char account_file[] = "/Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/data/test/account.csv";
//    char trans_file[] = "/Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/data/scale1/transfer.csv";
//    char account_file[] = "/Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/data/scale1/account.csv";
    char trans_file[] = "/Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/data/scale10/transfer.csv";
    char account_file[] = "/Users/ykddd/Desktop/com/CYCLE_DETECT_2021BDCI/data/scale10/account.csv";
//
//    if (argc != 4) {
//        std::cerr << "args error!" << std::endl;
//        return 0;
//    }
//
//    auto account_file = argv[1];
//    auto trans_file = argv[2];
//    auto res_file = argv[3];

    auto account_fd = open(account_file, O_RDONLY);
    auto account_file_len = lseek(account_fd, 0, SEEK_END);
    auto account_data = (char *) mmap(nullptr, account_file_len, PROT_READ, MAP_PRIVATE, account_fd, 0);
    close(account_fd);
    auto trans_fd = open(trans_file, O_RDONLY);
    auto trans_file_len = lseek(trans_fd, 0, SEEK_END);
    auto trans_data = (char *) mmap(nullptr, trans_file_len, PROT_READ, MAP_PRIVATE, trans_fd, 0);
    close(trans_fd);

    std::cout << "ReadFile cost " << Duration(time_begin) << "ms" << std::endl;
    time_begin = TimeNow();

    bool is_s1 = account_file_len == 14331229;
    account_num = is_s1 ? 998140 : 7200880;
    edge_num = is_s1 ? 28940477 : 286818003;


//    mi_option_enable(mi_option_t::mi_option_large_os_pages);
//
//    account = static_cast<uint64_t *>(mi_malloc(sizeof(uint64_t) * account_num));
//    forward = static_cast<Transfer *>(mi_malloc(sizeof(Transfer) * edge_num));
//    forward_src = static_cast<uint32_t *>(mi_malloc(sizeof(uint32_t) * edge_num));
//    forward_edge_index = static_cast<uint32_t *>(mi_malloc(sizeof(uint32_t) * edge_num + 1));
//    forward_dst = static_cast<uint32_t *>(mi_malloc(sizeof(uint32_t) * edge_num));
//    forward_ts = static_cast<uint64_t *>(mi_malloc(sizeof(uint64_t) * edge_num));
//    forward_value = static_cast<uint32_t *>(mi_malloc(sizeof(uint32_t) * edge_num));
//
//    backward_src = static_cast<uint32_t *>(mi_malloc(sizeof(uint32_t) * edge_num));
//    backward_dst = static_cast<uint32_t *>(mi_malloc(sizeof(uint32_t) * edge_num));
//    backward_ts = static_cast<uint64_t *>(mi_malloc(sizeof(uint64_t) * edge_num));
//    backward_value = static_cast<uint32_t *>(mi_malloc(sizeof(uint32_t) * edge_num));
//    backward_edge_index = static_cast<uint32_t *>(mi_malloc(sizeof(uint32_t) * edge_num + 1));

    account = tbb::scalable_allocator<uint64_t>().allocate(account_num);
    forward = tbb::scalable_allocator<Transfer>().allocate(edge_num);
    forward_src = tbb::scalable_allocator<uint32_t>().allocate(edge_num);
    forward_edge_index = tbb::scalable_allocator<uint32_t>().allocate(edge_num + 1);
    forward_dst = tbb::scalable_allocator<uint32_t>().allocate(edge_num);
    forward_ts = tbb::scalable_allocator<uint64_t>().allocate(edge_num);
    forward_value = tbb::scalable_allocator<uint32_t>().allocate(edge_num);

    forward_prune_index_beg = tbb::scalable_allocator<uint32_t>().allocate(edge_num + 1);
    forward_prune_index_end = tbb::scalable_allocator<uint32_t>().allocate(edge_num + 1);

    backward = tbb::scalable_allocator<Transfer>().allocate(edge_num);
    backward_src = tbb::scalable_allocator<uint32_t>().allocate(edge_num);
    backward_dst = tbb::scalable_allocator<uint32_t>().allocate(edge_num);
    backward_ts = tbb::scalable_allocator<uint64_t>().allocate(edge_num);
    backward_value = tbb::scalable_allocator<uint32_t>().allocate(edge_num);

    backward_prune_index_beg = tbb::scalable_allocator<uint32_t>().allocate(edge_num + 1);
    backward_prune_index_end = tbb::scalable_allocator<uint32_t>().allocate(edge_num + 1);

    backward_edge_index = tbb::scalable_allocator<uint32_t>().allocate(edge_num + 1);

//    account_offset = tbb::scalable_allocator<uint32_t>().allocate(account_num + 1);
    account_thread_offset = new uint32_t *[thread_num + 1];
    for (auto i = 0; i < thread_num + 1; ++i) {
        account_thread_offset[i] = tbb::scalable_allocator<uint32_t>().allocate(account_num + 1);
    }

    auto *beg = account_data;
    id_map.reserve(account_num);
    for (uint32_t i = 0; i < account_num; ++i) {
        GET_UINT64(account[i], beg, lf);
        id_map[account[i]] = i;
    }
    munmap(account_data, account_file_len);

    std::cout << "last_account_id:" << account[account_num - 1] << std::endl;
    std::cout << "GetAccount cost " << Duration(time_begin) << "ms" << std::endl;
    time_begin = TimeNow();


    std::vector<std::thread> threads;
    auto line_counter = new uint32_t[thread_num + 1]{};
    auto get_line_num = [&](size_t tid, off_t beg, off_t end) {
        auto task_begin = trans_data + beg;
        auto task_end = trans_data + end;
        if (tid != 0) {
            while (*task_begin != lf) {
                ++task_begin;
            }
            ++task_begin;
        }
        if (tid != thread_num - 1) {
            while (*task_end != lf) {
                ++task_end;
            }
            ++task_end;
        }
        for (; task_begin < task_end; ++task_begin) {
            if (*task_begin == lf) {
                ++line_counter[tid + 1];
                task_begin += 28;
            }
        }
    };
    for (size_t i = 0; i < thread_num; ++i) {
        threads.emplace_back(get_line_num,
                             i,
                             trans_file_len * i / thread_num,
                             trans_file_len * (i + 1) / thread_num);
    }
    for (auto &thread : threads) {
        thread.join();
    }
    for (size_t i = 0; i < thread_num; ++i) {
        line_counter[i + 1] += line_counter[i];
    }
    std::cout << "edge_num: " << line_counter[thread_num] << std::endl;
    std::cout << "GetLineOffset cost " << Duration(time_begin) << "ms" << std::endl;


    auto get_transfer = [&](size_t tid, off_t beg, off_t end) {
        auto task_begin = trans_data + beg;
        auto task_end = trans_data + end;
        if (tid != 0) {
            while (*task_begin != lf) {
                ++task_begin;
            }
            ++task_begin;
        }
        if (tid != thread_num - 1) {
            while (*task_end != lf) {
                ++task_end;
            }
            ++task_end;
        }
        uint64_t from;
        uint64_t to;
        uint64_t time_stamp;
        uint32_t amount;
        uint32_t cur_line_num = line_counter[tid];
        uint64_t pre_from;
        auto tmp_task_beg = task_begin;
        GET_UINT64(pre_from, tmp_task_beg, comma);
        uint32_t mapped_from = id_map[pre_from];

        for (; task_begin < task_end; ++task_begin) {
            GET_UINT64(from, task_begin, comma);
            GET_UINT64(to, task_begin, comma);
            GET_UINT64(time_stamp, task_begin, comma);
            amount = 0;
            for (; *task_begin != lf; ++task_begin) {
                if (*task_begin == dot) {
                    ++task_begin;
                }
                amount = (*task_begin - zero) + 10 * amount;
            }
            if (pre_from != from) {
                mapped_from = id_map[from];
                pre_from = from;
            }
            forward[cur_line_num].amount = amount;
            forward[cur_line_num].src_id = mapped_from;
            forward[cur_line_num].dst_id = id_map[to];
            forward[cur_line_num].time_stamp = time_stamp;
            account_thread_offset[tid + 1][forward[cur_line_num].dst_id + 1]++;
            ++cur_line_num;
        }
    };
    threads.clear();
    for (size_t i = 0; i < thread_num; ++i) {
        threads.emplace_back(get_transfer,
                             i,
                             trans_file_len * i / thread_num,
                             trans_file_len * (i + 1) / thread_num);
    }
    for (auto &thread : threads) {
        thread.join();
    }
    munmap(trans_data, trans_file_len);

    std::cout << "GetTransfer cost " << Duration(time_begin) << "ms" << std::endl;
    time_begin = TimeNow();

    for (auto i = 0; i < thread_num; ++i) {
        tbb::parallel_for(tbb::blocked_range<uint32_t>(0, account_num + 1),
                          [&](tbb::blocked_range<uint32_t> r) {
                              for (uint32_t j = r.begin(); j < r.end(); ++j) {
                                  account_thread_offset[i + 1][j] += account_thread_offset[i][j];
                              }
                          });
    }

    for (auto i = 0; i < account_num; ++i) {
        account_thread_offset[thread_num][i + 1] += account_thread_offset[thread_num][i];
    }

    auto account_offset = account_thread_offset[thread_num];
    auto build_backward = [&](size_t tid, uint32_t beg, uint32_t end) {
        for (uint32_t line = beg; line < end; ++line) {
            auto &cur_forward = forward[line];
            auto dst_id = cur_forward.dst_id;
            backward[account_thread_offset[tid][dst_id + 1] + account_offset[dst_id]] = cur_forward;
            account_thread_offset[tid][dst_id + 1]++;
        }
    };
    threads.clear();
    for (size_t i = 0; i < thread_num; ++i) {
        threads.emplace_back(build_backward,
                             i,
                             line_counter[i],
                             line_counter[i + 1]);
    }

    for (auto &thread : threads) {
        thread.join();
    }

    std::cout << forward[edge_num - 1].time_stamp << " " << forward[edge_num - 1].amount << std::endl;

    std::cout << "Construct backward " << Duration(time_begin) << "ms" << std::endl;
    time_begin = TimeNow();


    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, edge_num),
                      [&](tbb::blocked_range<uint32_t> r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              forward_src[i] = forward[i].src_id;
                              backward_src[i] = backward[i].dst_id;
                          }
                      });

    tbb::parallel_for(tbb::blocked_range<uint32_t>(1, edge_num),
                      [&](tbb::blocked_range<uint32_t> r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              for (uint32_t j = forward_src[i - 1] + 1; j <= forward_src[i]; ++j) {
                                  forward_edge_index[j] = i;
                              }
                              for (uint32_t j = backward_src[i - 1] + 1; j <= backward_src[i]; ++j) {
                                  backward_edge_index[j] = i;
                              }
                          }
                      });
    forward_edge_index[account_num] = edge_num;
    backward_edge_index[account_num] = edge_num;

    std::cout << "Make forward/backward index cost " << Duration(time_begin) << "ms" << std::endl;
    time_begin = TimeNow();

    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, account_num),
                      [&](tbb::blocked_range<uint32_t> r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              ska_sort(forward + forward_edge_index[i],
                                       forward + forward_edge_index[i + 1],
                                       [](const Transfer &it) { return it.amount; });
                              ska_sort(backward + backward_edge_index[i],
                                       backward + backward_edge_index[i + 1],
                                       [](const Transfer &it) { return it.amount; });
                          }
                      });
    std::cout << "Sort forward/backward trans cost " << Duration(time_begin) << "ms" << std::endl;
    time_begin = TimeNow();

    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, edge_num),
                      [&](tbb::blocked_range<uint32_t> r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              auto &cur_edge = forward[i];
                              forward_prune_index_beg[i] =
                                      std::distance(forward,
                                                    std::upper_bound(forward + forward_edge_index[cur_edge.dst_id],
                                                                     forward + forward_edge_index[cur_edge.dst_id + 1],
                                                                     cur_edge.amount * 9, Transfer::cmp_upper));
                              forward_prune_index_end[i] =
                                      std::distance(forward,
                                                    std::lower_bound(forward + forward_edge_index[cur_edge.dst_id],
                                                                     forward + forward_edge_index[cur_edge.dst_id + 1],
                                                                     cur_edge.amount * 11, Transfer::cmp_lower));
                          }
                      });

    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, edge_num),
                      [&](tbb::blocked_range<uint32_t> r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              auto &cur_edge = backward[i];
                              backward_prune_index_beg[i] =
                                      std::distance(backward,
                                                    std::upper_bound(backward + backward_edge_index[cur_edge.src_id],
                                                                     backward +
                                                                     backward_edge_index[cur_edge.src_id + 1],
                                                                     cur_edge.amount * 10, Transfer::cmp_back_upper));
                              backward_prune_index_end[i] =
                                      std::distance(backward,
                                                    std::lower_bound(backward + backward_edge_index[cur_edge.src_id],
                                                                     backward +
                                                                     backward_edge_index[cur_edge.src_id + 1],
                                                                     cur_edge.amount * 10, Transfer::cmp_back_lower));
                          }
                      });


//    return 0;

    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, edge_num),
                      [&](tbb::blocked_range<uint32_t> r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              forward_dst[i] = forward[i].dst_id;
                              forward_ts[i] = forward[i].time_stamp;
                              forward_value[i] = forward[i].amount;

                              backward_dst[i] = backward[i].src_id;
                              backward_ts[i] = backward[i].time_stamp;
                              backward_value[i] = backward[i].amount;
                          }
                      });
    std::cout << "Flatten trans cost " << Duration(time_begin) << "ms" << std::endl;
    time_begin = TimeNow();

    CounterType counter(4);
    tbb::affinity_partitioner ap;
    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, account_num),
                      [&](tbb::blocked_range<uint32_t> &r) {
                          auto back_rec = new BackRec[5000];
                          auto in_back = new bool[account_num]{};
                          char local_res[300];
                          CounterType::reference my_counter = counter.local();
                          uint32_t back_recode_num = 0;
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              BackFindThree(i, back_rec, in_back, back_recode_num);
                              if (back_recode_num == 0) {
                                  continue;
                              }
                              ForwardFindThree(i, back_rec, in_back, local_res, back_recode_num, my_counter);
                          }
                          delete[]back_rec;
                          delete[]in_back;
                      }, ap);

    std::vector<uint32_t> cyclye_num(4, 0);
    for (auto &item:counter) {
        cyclye_num[0] += item[0];
        cyclye_num[1] += item[1];
        cyclye_num[2] += item[2];
        cyclye_num[3] += item[3];
    }
    std::cout <<
              cyclye_num[0] << " " <<
              cyclye_num[1] << " " <<
              cyclye_num[2] << " " <<
              cyclye_num[3] << std::endl;

    auto cycyle_num = std::accumulate(cyclye_num.begin(), cyclye_num.end(), 0U);;
    std::cout << "cycle total num: " << cycyle_num << std::endl;
    std::cout << "Find cycle cost " << Duration(time_begin) << "ms" << std::endl;
    time_begin = TimeNow();


    auto res_len = res_buff_offset.load();
    int fd = open(res_file, O_RDWR | O_CREAT | O_TRUNC, 0666);
    std::cout << "result data length: " << res_len << std::endl;
    char *mmap_res = (char *) mmap(NULL, res_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    lseek(fd, res_len - 1, SEEK_SET);
    write(fd, " ", 1);

    memcpy(mmap_res, res_data, res_len);
    close(fd);
    munmap(mmap_res, res_len);
    std::cout << "Export res cost " << Duration(time_begin) << "ms" << std::endl;
    PerfUtils::PerfRaii::Report();
    return 0;
}



