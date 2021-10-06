//
// Created by ykddd on 2021/9/26.
//

#include "cycle_detecter.h"
#include "file_helper.h"
#include "string_utils.h"
#include "tbb/parallel_sort.h"
#include "tbb/parallel_for_each.h"
#include "tbb/enumerable_thread_specific.h"

#include <vector>
#include <numeric>

#define IfTrueThenDo(cond, ...)      \
    if (cond) {                      \
     __VA_ARGS__;                    \
    }

#define SkipTs(last_index, cur_index, node)                   \
    for (cur_index = forward_edge_index[node];                \
         cur_index < forward_edge_index[node + 1];            \
         ++cur_index) {                                       \
        IfTrueThenDo(                                         \
            forward_ts[last_index] < forward_ts[cur_index],   \
            break;                                            \
        );                                                    \
}

#define SkipAmt(last_index, cur_index)                                    \
    if (forward_value[last_index] * 9 > forward_value[cur_index] * 10 ||  \
        forward_value[last_index] * 11 < forward_value[cur_index] * 10) { \
            continue;                                                     \
    }

#define SkipDupliNode(cond)       \
    IfTrueThenDo(cond, continue;) \


void CycleDetecter::SortTransfer() {
    PerfThis(__FUNCTION__);
    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, account_num),
                      [&](tbb::blocked_range<uint32_t> r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              std::sort(forward_trans + forward_edge_index[i],
                                        forward_trans + forward_edge_index[i + 1],
                                        Transfer::cmp_transfer_ts);
                          }
                      });

}

void CycleDetecter::SortBackTransfer() {
    PerfThis(__FUNCTION__);
    tbb::parallel_sort(forward_trans, forward_trans + edge_num, Transfer::cmp_back_trans);
}

void CycleDetecter::FlattenTrans() {
    PerfThis(__FUNCTION__);
    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, edge_num),
                      [&](tbb::blocked_range<uint32_t> r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              // not need to flatten forward src
                              forward_dst[i] = forward_trans[i].dst_id;
                              forward_ts[i] = forward_trans[i].time_stamp;
                              forward_value[i] = forward_trans[i].amount;
                          }
                      });
}

void CycleDetecter::FlattenBackTrans() {
    PerfThis(__FUNCTION__);
    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, edge_num),
                      [&](tbb::blocked_range<uint32_t> r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              backward_src[i] = forward_trans[i].dst_id;
                              backward_dst[i] = forward_trans[i].src_id;
                              backward_ts[i] = forward_trans[i].time_stamp;
                              backward_value[i] = forward_trans[i].amount;
                          }
                      });
//    for(int i = 0 ; i < 100; ++i) {
//        std::cout << backward_src[i] << " ";
//    }
//    std::cout << std::endl;
}

void CycleDetecter::MakeEdgeIndex() {
    PerfThis(__FUNCTION__);
    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, edge_num),
                      [&](tbb::blocked_range<uint32_t> r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              forward_src[i] = forward_trans[i].src_id;
                          }
                      });


    forward_edge_index[0] = 0;
    tbb::parallel_for(tbb::blocked_range<uint32_t>(1, edge_num),
                      [&](tbb::blocked_range<uint32_t> r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              for (uint32_t j = forward_src[i - 1] + 1; j < forward_src[i] + 1; ++j) {
                                  forward_edge_index[j] = i;
                              }
                          }
                      });

    forward_edge_index[account_num] = edge_num;

}

void CycleDetecter::MakeBackEdgeIndex() {
    PerfThis(__FUNCTION__);
    backward_edge_index[0] = 0;

    tbb::parallel_for(tbb::blocked_range<uint32_t>(1, edge_num),
                      [&](tbb::blocked_range<uint32_t> r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              for (uint32_t j = backward_src[i - 1] + 1; j < backward_src[i] + 1; ++j) {
                                  backward_edge_index[j] = i;
                              }
                          }
                      });

    backward_edge_index[account_num] = edge_num;
}

void CycleDetecter::FindCycle() {
    PerfThis(__FUNCTION__);
    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, account_num),
                      [&](tbb::blocked_range<uint32_t> r) {
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              NativeSixFor(i);
                          }
                      });
//    for (int i = 0; i < account_num; ++i) {
//        NativeSixFor(i);
//    }
    std::cout <<
              cycyle_num_len_3.load() << " " <<
              cycyle_num_len_4.load() << " " <<
              cycyle_num_len_5.load() << " " <<
              cycyle_num_len_6.load() << std::endl;

    cycyle_num = cycyle_num_len_3.load() + cycyle_num_len_4.load() +
                 cycyle_num_len_5.load() + cycyle_num_len_6.load();
    std::cout << "cycle total num: " << cycyle_num << std::endl;
}

void CycleDetecter::NativeSixFor(uint32_t cur_node) {
    char local_res[256];
//    if (cur_node % 10000 == 0) {
//        std::cout << "cur node: " << cur_node << std::endl;
//    }
    uint32_t node_1, node_2, node_3, node_4, node_5;
    uint32_t j, k, m, n, u;
    std::array<uint32_t, 6> res_index{cur_node};
    for (uint32_t i = forward_edge_index[cur_node]; i < forward_edge_index[cur_node + 1]; ++i) {
        node_1 = forward_dst[i];
        SkipTs(i, j, node_1);
        for (; j < forward_edge_index[node_1 + 1]; ++j) {
            SkipAmt(i, j);
            node_2 = forward_dst[j];
            SkipDupliNode(node_2 == cur_node);
            SkipTs(j, k, node_2);
            for (; k < forward_edge_index[node_2 + 1]; ++k) {
                SkipAmt(j, k);
                node_3 = forward_dst[k];
                IfTrueThenDo(node_3 == cur_node,
                             res_index[1] = i;
                             res_index[2] = j;
                             ExportRes(res_index, 3, local_res);
                             ++cycyle_num_len_3;
                             continue;)
                SkipDupliNode(node_3 == node_1);
                SkipTs(k, m, node_3);
                for (; m < forward_edge_index[node_3 + 1]; ++m) {
                    SkipAmt(k, m);
                    node_4 = forward_dst[m];
                    IfTrueThenDo(node_4 == cur_node,
                                 res_index[1] = i;
                                 res_index[2] = j;
                                 res_index[3] = k;
                                 ExportRes(res_index, 4, local_res);
                                 ++cycyle_num_len_4;
                                 continue;)
                    SkipDupliNode(node_4 == node_1 || node_4 == node_2);
                    SkipTs(m, n, node_4);
                    for (; n < forward_edge_index[node_4 + 1]; ++n) {
                        SkipAmt(m, n);
                        node_5 = forward_dst[n];
                        IfTrueThenDo(node_5 == cur_node,
                                     ++cycyle_num_len_5;
                                     res_index[1] = i;
                                     res_index[2] = j;
                                     res_index[3] = k;
                                     res_index[4] = m;
                                     ExportRes(res_index, 5, local_res);
                                     continue;)
                        SkipDupliNode(node_5 == node_1 || node_5 == node_2 || node_5 == node_3);
                        SkipTs(n, u, node_5);
                        for (; u < forward_edge_index[node_5 + 1]; ++u) {
                            SkipAmt(n, u);
                            IfTrueThenDo(forward_dst[u] == cur_node,
                                         ++cycyle_num_len_6;
                                         res_index[1] = i;
                                         res_index[2] = j;
                                         res_index[3] = k;
                                         res_index[4] = m;
                                         res_index[5] = n;
                                         ExportRes(res_index, 6, local_res);)
                        }
                    }
                }
            }
        }
    }
}

void CycleDetecter::ExportRes(const std::array<uint32_t, 6> &res_index,
                              uint32_t cycle_len,
                              char *tmp_buffer) {
//    PerfThis(__FUNCTION__);
    //(10)-[100,10.50]->(11)-[102,11.52]->(12)-[104,11.54]->(10)
    uint32_t buf_idx = 0;
    tmp_buffer[buf_idx++] = '(';
    buf_idx += StrUtils::u64toa_jeaiii(account[res_index[0]], tmp_buffer + buf_idx);
    tmp_buffer[buf_idx++] = ')';
    tmp_buffer[buf_idx++] = '-';
    tmp_buffer[buf_idx++] = '[';
    buf_idx += StrUtils::u64toa_jeaiii(forward_ts[res_index[0]], tmp_buffer + buf_idx);
    tmp_buffer[buf_idx++] = ',';
    buf_idx += StrUtils::u64toa_jeaiii(forward_value[res_index[0]] / 100U, tmp_buffer + buf_idx);
    tmp_buffer[buf_idx++] = '.';
    buf_idx += StrUtils::u64toa_jeaiii(forward_value[res_index[0]] % 100U, tmp_buffer + buf_idx);
    tmp_buffer[buf_idx++] = ']';
    tmp_buffer[buf_idx++] = '-';
    tmp_buffer[buf_idx++] = '>';
    for (size_t i = 1; i < cycle_len; ++i) {
        ExportResImpl(res_index[i], tmp_buffer, buf_idx);
    }
    tmp_buffer[buf_idx++] = '(';
    buf_idx += StrUtils::u64toa_jeaiii(account[res_index[0]], tmp_buffer + buf_idx);
    tmp_buffer[buf_idx++] = ')';
    tmp_buffer[buf_idx++] = '\n';
    off_t cur_res_offset = res_buff_offset.fetch_and_add(buf_idx);
    memcpy(res_data + cur_res_offset, tmp_buffer, buf_idx);
}

void CycleDetecter::ExportResImpl(uint32_t idx, char *tmp_buffer, uint32_t &buf_idx) {
    tmp_buffer[buf_idx++] = '(';
    buf_idx += StrUtils::u64toa_jeaiii(forward_dst[idx], tmp_buffer + buf_idx);
    tmp_buffer[buf_idx++] = ')';
    tmp_buffer[buf_idx++] = '-';
    tmp_buffer[buf_idx++] = '[';
    buf_idx += StrUtils::u64toa_jeaiii(forward_ts[idx], tmp_buffer + buf_idx);
    tmp_buffer[buf_idx++] = ',';
    buf_idx += StrUtils::u64toa_jeaiii(forward_value[idx] / 100U, tmp_buffer + buf_idx);
    tmp_buffer[buf_idx++] = '.';
    buf_idx += StrUtils::u64toa_jeaiii(forward_value[idx] % 100U, tmp_buffer + buf_idx);
    tmp_buffer[buf_idx++] = ']';
    tmp_buffer[buf_idx++] = '-';
    tmp_buffer[buf_idx++] = '>';
}

void CycleDetecter::ExportResToFile() {
    PerfThis(__FUNCTION__);
    auto res_len = res_buff_offset.load();
    int fd = open(output_file, O_RDWR | O_CREAT | O_TRUNC, 0666);

    assert(fd >= 0);

    std::cout << "result data length: " << res_len << std::endl;

    char *mmap_res = (char *) mmap(NULL, res_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    lseek(fd, res_len - 1, SEEK_SET);
    write(fd, " ", 1);

    memcpy(mmap_res, res_data, res_len);
    close(fd);
    munmap(mmap_res, res_len);
}

void CycleDetecter::Run() {
    PerfThis(__FUNCTION__);

    // 1:998140, 10:7200880
    account_num = FileHelper::GetAccountNum(account_data, account_file_len);

    account = new uint64_t[account_num];
    FileHelper::GetAccount(account_data, account_num, account, id_map);

    // 1:28940477, 10:286818003
    edge_num = FileHelper::GetEdgeNum(trans_data, trans_file_len);


    forward_trans = new Transfer[edge_num];

    FileHelper::GetTransferFromFile(trans_data, trans_file_len, edge_num,
                                    forward_trans, id_map);
    forward_src = new uint32_t[edge_num];
    forward_dst = new uint32_t[edge_num];
    forward_ts = new int64_t[edge_num];
    forward_value = new uint32_t[edge_num];
    forward_edge_index = new uint32_t[account_num + 2];

//    backward_src = new uint32_t[edge_num];
//    backward_dst = new uint32_t[edge_num];
//    backward_ts = new int64_t[edge_num];
//    backward_value = new uint32_t[edge_num];
//    backward_edge_index = new uint32_t[account_num + 2];

    MakeEdgeIndex();
    SortTransfer();
    FlattenTrans();

//    SortBackTransfer();
//    FlattenBackTrans();
//    MakeBackEdgeIndex();

    res_data = new char[335620 * 300];
    FindCycle();
    ExportResToFile();
}




