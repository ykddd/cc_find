//
// Created by ykddd on 2021/9/25.
//

#include "file_helper.h"
#include "perf_utils.h"
#include "tbb/parallel_for.h"
#include "tbb/parallel_sort.h"
#include "tbb/parallel_for_each.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/enumerable_thread_specific.h"
#include "tbb/concurrent_priority_queue.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <numeric>
#include <vector>

#define GET_UINT64(num, cur, deli)            \
    do {                                      \
        num = 0;                              \
        while(*cur != deli){                  \
            num = (*cur - '0') + 10 * num;    \
            ++cur;                            \
        }                                     \
        ++cur;                                \
    } while(false)                            \



#define GET_UINT64_V2(num, index, deli, data)       \
    do {                                            \
        num = 0;                                    \
        while(data[i] != deli){                     \
            num = (data[i] - '0') + 10 * num;       \
            ++index;                                \
        }                                           \
        ++index;                                    \
    } while(false)                                  \

#define GET_INT64(num, cur)                   \
    do {                                      \
        num = 0;                              \
        bool sign = (*cur == '-');            \
        if(sign) {                            \
            ++cur;                            \
        }                                     \
        while(*cur != ','){                   \
            num = (*cur - '0') + 10 * num;    \
            ++cur;                            \
        }                                     \
        ++cur;                                \
        num = sign ? -num : num;              \
    } while(false)                            \

#define GET_DOUBLE(num1, num2, amount, cur)   \
    do {                                      \
        num1 = 0;                             \
        while(*cur != '.'){                   \
            num1 = (*cur - '0') + 10 * num1;  \
            ++cur;                            \
        }                                     \
        ++cur;                                \
        num2 = 0;                             \
        amount = 1.0;                         \
        while(*cur != '\n'){                  \
            amount = 0.1 * amount;            \
            num2 = (*cur - '0') + 10 * num2;  \
            ++cur;                            \
        }                                     \
        amount = num1 + amount * num2;        \
    } while(false)                            \

namespace {
    constexpr char lf = '\n';
    constexpr char comma = ',';
    constexpr char dot = '.';
}

uint32_t FileHelper::GetAccountNum(char *data, off_t file_len) {
    PerfThis(__FUNCTION__);
    uint32_t account_num;

    // parallel for
    typedef tbb::enumerable_thread_specific<uint32_t> CounterType;
    CounterType counter(0);
    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, file_len),
                      [&](tbb::blocked_range<uint32_t> r) {
                          CounterType::reference my_counter = counter.local();
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              if (data[i] == lf) {
                                  ++my_counter;
                              }
                          }
                      });
    account_num = std::accumulate(counter.begin(), counter.end(), 0U);
    std::cout << "account_num:" << account_num << std::endl;
    return account_num;
}

void FileHelper::GetAccount(char *account_data, uint32_t account_num,
                            uint64_t *account, IdMapType &id_map) {
    PerfThis(__FUNCTION__);
    auto *beg = account_data;
    uint64_t account_id = 0;
    id_map.reserve(account_num);
    for (uint32_t i = 0; i < account_num; ++i) {
        GET_UINT64(account_id, beg, lf);
        account[i] = account_id;
        id_map[account_id] = i;
    }
    std::cout << "last_account_id: " << account_id << std::endl;
    std::cout << "id_map size: " << id_map.size() << std::endl;
}

uint32_t FileHelper::GetEdgeNum(char *data, off_t file_len) {
    PerfUtils::PerfRaii perf_raii{__FUNCTION__};
//    tbb::task_scheduler_init schedulerInit(8);
    uint32_t edge_num = 0;
    typedef tbb::enumerable_thread_specific<uint32_t> CounterType;
    CounterType counter(0);
    tbb::parallel_for(tbb::blocked_range<off_t>(0, file_len),
                      [&](tbb::blocked_range<off_t> r) {
                          CounterType::reference my_counter = counter.local();
                          for (off_t i = r.begin(); i < r.end(); ++i) {
                              if (data[i] == lf) {
                                  ++my_counter;
                                  i += 28;
                              }
                          }
                      });
    edge_num = std::accumulate(counter.begin(), counter.end(), 0U);
    std::cout << "edge_num: " << edge_num << std::endl;
    return edge_num;
}

void FileHelper::GetTransferFromFile(const char *trans_data,
                                     off_t file_len,
                                     uint32_t edge_num,
                                     Transfer *forward,
                                     IdMapType &id_map) {
    PerfThis(__FUNCTION__);
    tbb::atomic<uint32_t> edge_index{0};
    tbb::parallel_for(tbb::blocked_range<off_t>(0, file_len),
                      [&](tbb::blocked_range<off_t> r) {
                          uint64_t from;
                          uint64_t to;
                          uint64_t time_stamp;
                          uint32_t amount;

                          auto beg = r.begin();
                          while (trans_data[beg] != lf && beg != 0) {
                              ++beg;
                          }
                          if (beg != 0) {
                              ++beg;
                          }
                          auto end = r.end();
                          while (trans_data[end] != lf && end != file_len) {
                              ++end;
                          }
                          auto data = trans_data + beg;
                          for (off_t i = 0; i < end - beg; ++i) {
                              GET_UINT64_V2(from, i, comma, data);
                              GET_UINT64_V2(to, i, comma, data);
                              GET_UINT64_V2(time_stamp, i, comma, data);
                              amount = 0;
                              while (data[i] != lf) {
                                  if (data[i] == dot) {
                                      ++i;
                                  }
                                  amount = (data[i] - '0') + 10 * amount;
                                  ++i;
                              }
                              uint32_t cur_index = edge_index.fetch_and_add(1);
                              forward[cur_index] = Transfer(amount, id_map[from], id_map[to], time_stamp);
                          }
                      }
    );
    return;

    auto *beg = trans_data;
    uint64_t from;
    uint64_t to;
    uint64_t time_stamp;
    uint32_t amount;
    uint64_t prev_id;
    uint32_t index = 0;
    GET_UINT64(prev_id, trans_data, comma);

    for (uint32_t i = 0; i < edge_num; ++i) {
        GET_UINT64(from, beg, comma);
        GET_UINT64(to, beg, comma);
        GET_UINT64(time_stamp, beg, comma);

        amount = 0;
        while (*beg != lf) {
            if (*beg == dot) {
                ++beg;
            }
            amount = (*beg - '0') + 10 * amount;
            ++beg;
        }
        ++beg;
        if (from != prev_id) {
            ++index;
            prev_id = from;
        }
        forward[i] = Transfer(amount, index, to, time_stamp);
    }

    tbb::parallel_for(tbb::blocked_range<off_t>(0, edge_num),
                      [&](tbb::blocked_range<off_t> r) {
                          for (off_t i = r.begin(); i < r.end(); ++i) {
                              forward[i].dst_id = id_map[forward[i].dst_id];
                          }
                      });
}
