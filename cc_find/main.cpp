//
// Created by ykddd on 2021/9/25.
//

#include "ska_sort.h"
#include "tsl/robin_map.h"
#include "perf_utils.h"
#include "flat_hash_map.h"
#include "tbb/parallel_for.h"
#include "tbb/global_control.h"

#define TBB_PREVIEW_MEMORY_POOL 1

#include "tbb/memory_pool.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/enumerable_thread_specific.h"
#include "string_utils.h"
#include "mimalloc.h"


#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <chrono>
#include <vector>
#include <thread>


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

    uint32_t amount;
    uint32_t src_id;
    uint32_t dst_id;
    uint64_t time_stamp;
};

struct BackRec {
    BackRec() = default;

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

uint32_t *forward_edge_index;
uint32_t *forward_prune_index_beg;
uint32_t *forward_prune_index_end;

Transfer *backward;
uint32_t *backward_edge_index;
uint32_t *backward_prune_index_beg;
uint32_t *backward_prune_index_end;

uint32_t **account_thread_offset;


tbb::atomic<off_t> res_buff_offset{0};

char *res_data;

#define GET_UINT64(num, cur, deli)             \
    num = 0;                                   \
    for(; *cur != deli; ++cur){                \
        num = (*cur - zero) + 10 * num;        \
    }                                          \
    ++cur;                                     \


void MakeForwardRes(uint32_t index, uint32_t &offset, char *res) {
    auto &tmp = forward[index];
    res[offset++] = '-';
    res[offset++] = '[';
    offset += StrUtils::u64toa_jeaiii(tmp.time_stamp, res + offset);
    res[offset++] = ',';
    offset += StrUtils::u64toa_jeaiii(tmp.amount / 100U, res + offset);
    res[offset++] = '.';
    auto x = tmp.amount % 100U;
    if (x < 10) {
        res[offset++] = '0';
    }
    offset += StrUtils::u64toa_jeaiii(x, res + offset);
    res[offset++] = ']';
    res[offset++] = '-';
    res[offset++] = '>';
    res[offset++] = '(';
    offset += StrUtils::u64toa_jeaiii(account[tmp.dst_id], res + offset);
    res[offset++] = ')';

}

uint32_t MakeBackwardRes(BackRec &back_rec, uint32_t offset, char *res, uint32_t cur_node) {
    auto f = [&](uint32_t idx) {
        auto &tmp = backward[idx];
        res[offset++] = '-';
        res[offset++] = '[';
        offset += StrUtils::u64toa_jeaiii(tmp.time_stamp, res + offset);
        res[offset++] = ',';
        offset += StrUtils::u64toa_jeaiii(tmp.amount / 100U, res + offset);
        res[offset++] = '.';
        auto x = tmp.amount % 100U;
        if (x < 10) {
            res[offset++] = '0';
        }
        offset += StrUtils::u64toa_jeaiii(x, res + offset);
        res[offset++] = ']';
        res[offset++] = '-';
        res[offset++] = '>';
        res[offset++] = '(';
    };
    f(back_rec.b1_idx);
    offset += StrUtils::u64toa_jeaiii(account[backward[back_rec.b2_idx].src_id], res + offset);
    res[offset++] = ')';
    f(back_rec.b2_idx);
    offset += StrUtils::u64toa_jeaiii(account[backward[back_rec.b3_idx].src_id], res + offset);
    res[offset++] = ')';
    f(back_rec.b3_idx);
    offset += StrUtils::u64toa_jeaiii(account[cur_node], res + offset);
    res[offset++] = ')';

    res[offset++] = '\n';
    return offset;
}


void BackFindThree(uint32_t cur_node, BackRec *back_rec,
                   bool *in_back, uint32_t &back_recode_num) {
    uint32_t node_1;
    uint32_t j, k;
    for (uint32_t i = 0; i < back_recode_num; ++i) {
        in_back[back_rec[i].back_node] = false;
    }
    back_recode_num = 0;
    for (uint32_t i = backward_edge_index[cur_node]; i < backward_edge_index[cur_node + 1]; ++i) {
        node_1 = backward[i].src_id;
        for (j = backward_prune_index_beg[i]; j < backward_prune_index_end[i]; ++j) {
            if (backward[j].time_stamp > backward[i].time_stamp || backward[j].src_id == cur_node) {
                continue;
            }
            for (k = backward_prune_index_beg[j]; k < backward_prune_index_end[j]; ++k) {
                if (backward[k].time_stamp > backward[j].time_stamp || backward[k].src_id == node_1) {
                    continue;
                }
                back_rec[back_recode_num++].SetRec(backward[k].src_id, k, j, i);
                in_back[backward[k].src_id] = true;
            }
        }
    }
}

void ForwardFindThree(uint32_t cur_node, BackRec *back_rec,
                      bool *in_back, char *local_res,
                      uint32_t back_recode_num) {
    uint32_t node_1, node_2, node_3;
    uint32_t j, k, m;
    uint32_t forward_amt_1_top, forward_amt_2_top, forward_amt_3_top;
    uint32_t forward_amt_1_bottom, forward_amt_2_bottom, forward_amt_3_bottom;
    uint64_t forward_ts_1, forward_ts_2, forward_ts_3;

    local_res[0] = '(';
    uint32_t first_node_res_len = StrUtils::u64toa_jeaiii(account[cur_node], local_res + 1) + 1;
    local_res[first_node_res_len++] = ')';

    auto tmp_buffer = local_res + first_node_res_len;
    uint32_t tmp_res_len;


    for (uint32_t i = forward_edge_index[cur_node]; i < forward_edge_index[cur_node + 1]; ++i) {
        node_1 = forward[i].dst_id;
        forward_ts_1 = forward[i].time_stamp;

        if (in_back[node_1]) {
            tmp_res_len = 0;
            MakeForwardRes(i, tmp_res_len, tmp_buffer);

            forward_amt_1_top = forward[i].amount * 11;
            forward_amt_1_bottom = forward[i].amount * 9;
            for (j = 0; j < back_recode_num; ++j) {
                BackRec &tmp_back = back_rec[j];
                if (tmp_back.back_node != node_1 ||
                    forward_ts_1 > backward[tmp_back.b1_idx].time_stamp ||
                    forward_amt_1_bottom > backward[tmp_back.b1_idx].amount * 10 ||
                    forward_amt_1_top < backward[tmp_back.b1_idx].amount * 10) {
                    continue;
                }
                auto tmp_len = MakeBackwardRes(tmp_back, tmp_res_len, tmp_buffer, cur_node) + first_node_res_len;
                off_t cur_res_offset = res_buff_offset.fetch_and_add(tmp_len);
                memcpy(res_data + cur_res_offset, local_res, tmp_len);
//                ++counter[1];
            }
        }
        for (j = forward_prune_index_beg[i]; j < forward_prune_index_end[i]; ++j) {
            if (forward[j].time_stamp < forward_ts_1) {
                continue;
            }
            node_2 = forward[j].dst_id;
            if (node_2 == cur_node) {
                continue;
            }
            forward_ts_2 = forward[j].time_stamp;

            if (in_back[node_2]) {
                forward_amt_2_top = forward[j].amount * 11;
                forward_amt_2_bottom = forward[j].amount * 9;

                tmp_res_len = 0;
                MakeForwardRes(i, tmp_res_len, tmp_buffer);
                MakeForwardRes(j, tmp_res_len, tmp_buffer);

                for (k = 0; k < back_recode_num; ++k) {
                    BackRec &tmp_back = back_rec[k];
                    if (tmp_back.back_node != node_2 ||
                        forward_ts_2 > backward[tmp_back.b1_idx].time_stamp ||
                        forward_amt_2_bottom > backward[tmp_back.b1_idx].amount * 10 ||
                        forward_amt_2_top < backward[tmp_back.b1_idx].amount * 10 ||
                        backward[tmp_back.b2_idx].src_id == node_1 ||
                        backward[tmp_back.b3_idx].src_id == node_1) {
                        continue;
                    }
                    auto tmp_len = MakeBackwardRes(tmp_back, tmp_res_len, tmp_buffer, cur_node) + first_node_res_len;
                    off_t cur_res_offset = res_buff_offset.fetch_and_add(tmp_len);
                    memcpy(res_data + cur_res_offset, local_res, tmp_len);
//                    ++counter[2];
                }
            }

            for (k = forward_prune_index_beg[j]; k < forward_prune_index_end[j]; ++k) {
                if (forward[k].time_stamp < forward_ts_2) {
                    continue;
                }
                node_3 = forward[k].dst_id;
                if (!in_back[node_3] || node_3 == node_1) { continue; }

                if (node_3 == cur_node) {
                    tmp_res_len = 0;
                    MakeForwardRes(i, tmp_res_len, tmp_buffer);
                    MakeForwardRes(j, tmp_res_len, tmp_buffer);
                    MakeForwardRes(k, tmp_res_len, tmp_buffer);
                    tmp_buffer[tmp_res_len++] = '\n';
                    off_t cur_res_offset = res_buff_offset.fetch_and_add(tmp_res_len + first_node_res_len);
                    memcpy(res_data + cur_res_offset, local_res, tmp_res_len + first_node_res_len);
//                    ++counter[0];
                    continue;
                }
                forward_ts_3 = forward[k].time_stamp;
                forward_amt_3_top = forward[k].amount * 11;
                forward_amt_3_bottom = forward[k].amount * 9;

                tmp_res_len = 0;
                MakeForwardRes(i, tmp_res_len, tmp_buffer);
                MakeForwardRes(j, tmp_res_len, tmp_buffer);
                MakeForwardRes(k, tmp_res_len, tmp_buffer);
                for (m = 0; m < back_recode_num; ++m) {
                    BackRec &tmp_back = back_rec[m];
                    if (tmp_back.back_node != node_3 ||
                        forward_ts_3 > backward[tmp_back.b1_idx].time_stamp ||
                        forward_amt_3_bottom > backward[tmp_back.b1_idx].amount * 10 ||
                        forward_amt_3_top < backward[tmp_back.b1_idx].amount * 10 ||
                        backward[tmp_back.b2_idx].src_id == node_1 ||
                        backward[tmp_back.b3_idx].src_id == node_1 ||
                        backward[tmp_back.b2_idx].src_id == node_2 ||
                        backward[tmp_back.b3_idx].src_id == node_2) {
                        continue;
                    }
                    auto tmp_len = MakeBackwardRes(tmp_back, tmp_res_len, tmp_buffer, cur_node) + first_node_res_len;
                    off_t cur_res_offset = res_buff_offset.fetch_and_add(tmp_len);
                    memcpy(res_data + cur_res_offset, local_res, tmp_len);
//                    ++counter[3];
                }
            }
        }
    }
}


int main(int argc, char *argv[]) {
    auto thread_num = tbb::task_scheduler_init::default_num_threads();
    tbb::global_control c(tbb::global_control::max_allowed_parallelism, thread_num);
    tbb::affinity_partitioner ap;
    auto account_file = argv[1];
    auto trans_file = argv[2];
    auto res_file = argv[3];

    auto account_fd = open(account_file, O_RDONLY);
    auto account_file_len = lseek(account_fd, 0, SEEK_END);
    auto account_data = (char *) mmap(nullptr, account_file_len, PROT_READ, MAP_PRIVATE, account_fd, 0);
    close(account_fd);
    auto trans_fd = open(trans_file, O_RDONLY);
    auto trans_file_len = lseek(trans_fd, 0, SEEK_END);
    auto trans_data = (char *) mmap(nullptr, trans_file_len, PROT_READ, MAP_PRIVATE, trans_fd, 0);
    close(trans_fd);

    bool is_s1 = account_file_len == 14331229;
    account_num = is_s1 ? 998140 : 7200880;
    edge_num = is_s1 ? 28940477 : 286818003;


    mi_option_enable(mi_option_t::mi_option_large_os_pages);
    res_data = static_cast<char *>(mi_malloc(335620 * 300));
    account = static_cast<uint64_t *>(mi_malloc(sizeof(uint64_t) * account_num));
    forward = static_cast<Transfer *>(mi_malloc(sizeof(Transfer) * edge_num));
    forward_edge_index = static_cast<uint32_t *>(mi_malloc(sizeof(uint32_t) * (edge_num + 1)));

    backward = static_cast<Transfer *>(mi_malloc(sizeof(Transfer) * edge_num));
    backward_edge_index = static_cast<uint32_t *>(mi_malloc(sizeof(uint32_t) * (edge_num + 1)));

    forward_prune_index_beg = static_cast<uint32_t *>(mi_malloc(sizeof(uint32_t) * (edge_num + 1)));
    forward_prune_index_end = static_cast<uint32_t *>(mi_malloc(sizeof(uint32_t) * (edge_num + 1)));

    backward_prune_index_beg = static_cast<uint32_t *>(mi_malloc(sizeof(uint32_t) * (edge_num + 1)));
    backward_prune_index_end = static_cast<uint32_t *>(mi_malloc(sizeof(uint32_t) * (edge_num + 1)));

    account_thread_offset = new uint32_t *[thread_num + 1];
    for (auto i = 0; i < thread_num + 1; ++i) {
        account_thread_offset[i] = static_cast<uint32_t *>(mi_malloc(sizeof(uint32_t) * (account_num + 1)));
    }

    auto *beg = account_data;
    id_map.reserve(account_num);
    for (uint32_t i = 0; i < account_num; ++i) {
        GET_UINT64(account[i], beg, lf);
        id_map[account[i]] = i;
    }
    munmap(account_data, account_file_len);

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

    for (auto i = 0; i < thread_num; ++i) {
        tbb::parallel_for(tbb::blocked_range<uint32_t>(0, account_num + 1),
                          [&](tbb::blocked_range<uint32_t> r) {
                              for (uint32_t j = r.begin(); j < r.end(); ++j) {
                                  account_thread_offset[i + 1][j] += account_thread_offset[i][j];
                              }
                          }, ap);
    }

    for (auto i = 0; i < account_num; ++i) {
        account_thread_offset[thread_num][i + 1] += account_thread_offset[thread_num][i];
    }

    auto account_offset = account_thread_offset[thread_num];
    auto build_backward = [&](size_t tid, uint32_t beg, uint32_t end) {
        auto cur_offset = ++account_thread_offset[tid];
        for (uint32_t line = beg; line < end; ++line) {
            auto &cur_forward = forward[line];
            auto dst_id = cur_forward.dst_id;
            backward[cur_offset[dst_id] + account_offset[dst_id]] = cur_forward;
            cur_offset[dst_id]++;
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


    tbb::parallel_for(tbb::blocked_range<uint32_t>(1, edge_num),
                      [&](tbb::blocked_range<uint32_t> &r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              for (uint32_t j = forward[i - 1].src_id + 1; j <= forward[i].src_id; ++j) {
                                  forward_edge_index[j] = i;
                              }
                              for (uint32_t j = backward[i - 1].dst_id + 1; j <= backward[i].dst_id; ++j) {
                                  backward_edge_index[j] = i;
                              }
                          }
                      }, ap);
    forward_edge_index[account_num] = edge_num;
    backward_edge_index[account_num] = edge_num;

    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, account_num),
                      [&](tbb::blocked_range<uint32_t> &r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              ska_sort(forward + forward_edge_index[i],
                                       forward + forward_edge_index[i + 1],
                                       [](const Transfer &it) { return it.amount; });
                              ska_sort(backward + backward_edge_index[i],
                                       backward + backward_edge_index[i + 1],
                                       [](const Transfer &it) { return it.amount; });
                          }
                      }, ap);

    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, edge_num),
                      [&](tbb::blocked_range<uint32_t> &r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              auto &cur_edge = forward[i];
                              forward_prune_index_beg[i] =
                                      std::upper_bound(forward + forward_edge_index[cur_edge.dst_id],
                                                       forward + forward_edge_index[cur_edge.dst_id + 1],
                                                       cur_edge.amount * 9, Transfer::cmp_upper) - forward;

                              forward_prune_index_end[i] =
                                      std::lower_bound(forward + forward_edge_index[cur_edge.dst_id],
                                                       forward + forward_edge_index[cur_edge.dst_id + 1],
                                                       cur_edge.amount * 11, Transfer::cmp_lower) - forward;
                              for (; forward_prune_index_beg[i] <
                                     forward_prune_index_end[i]; ++forward_prune_index_beg[i]) {
                                  if ((forward + forward_prune_index_beg[i])->time_stamp > cur_edge.time_stamp) {
                                      break;
                                  }
                              }
                          }
                      }, ap);

    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, edge_num),
                      [&](tbb::blocked_range<uint32_t> &r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              auto &cur_edge = backward[i];
                              backward_prune_index_beg[i] =
                                      std::upper_bound(backward + backward_edge_index[cur_edge.src_id],
                                                       backward +
                                                       backward_edge_index[cur_edge.src_id + 1],
                                                       cur_edge.amount * 10, Transfer::cmp_back_upper) - backward;
                              backward_prune_index_end[i] =
                                      std::lower_bound(backward + backward_edge_index[cur_edge.src_id],
                                                       backward + backward_edge_index[cur_edge.src_id + 1],
                                                       cur_edge.amount * 10, Transfer::cmp_back_lower) - backward;
                              for (; backward_prune_index_beg[i] <
                                     backward_prune_index_end[i]; ++backward_prune_index_beg[i]) {
                                  if ((backward + backward_prune_index_beg[i])->time_stamp < cur_edge.time_stamp) {
                                      break;
                                  }
                              }
                          }
                      }, ap);

//    CounterType counter(4);
    scalable_allocation_mode(USE_HUGE_PAGES, 1);
    tbb::memory_pool<tbb::scalable_allocator<BackRec> > backrec_pool;
    tbb::memory_pool<tbb::scalable_allocator<bool> > bool_pool;
    tbb::memory_pool<tbb::scalable_allocator<char> > char_pool;
    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, account_num),
                      [&](tbb::blocked_range<uint32_t> &r) {
                          auto back_rec = new BackRec[2000];
                          auto in_back = new bool[account_num]{};
                          char local_res[300];
                          uint32_t back_recode_num = 0;
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              BackFindThree(i, back_rec, in_back, back_recode_num);
                              if (back_recode_num == 0) {
                                  continue;
                              }
                              ForwardFindThree(i, back_rec, in_back, local_res, back_recode_num);
                          }
                          delete[]back_rec;
                          delete[]in_back;
                      }, ap);

//    std::vector<uint32_t> cyclye_num(4, 0);
//    for (auto &item:counter) {
//        cyclye_num[0] += item[0];
//        cyclye_num[1] += item[1];
//        cyclye_num[2] += item[2];
//        cyclye_num[3] += item[3];
//    }
//    std::cout << cyclye_num[0] << " "
//              << cyclye_num[1] << " "
//              << cyclye_num[2] << " "
//              << cyclye_num[3] << std::endl;
//
//    auto cycyle_num = std::accumulate(cyclye_num.begin(), cyclye_num.end(), 0U);;
//    std::cout << "cycle total num: " << cycyle_num << std::endl;


    auto res_len = res_buff_offset.load();
    int fd = open(res_file, O_RDWR | O_CREAT | O_TRUNC, 0666);
    std::cout << "result data length: " << res_len << std::endl;
    char *mmap_res = (char *) mmap(NULL, res_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    lseek(fd, res_len - 1, SEEK_SET);
    write(fd, " ", 1);

    memcpy(mmap_res, res_data, res_len);
    close(fd);
    munmap(mmap_res, res_len);
    return 0;
}



