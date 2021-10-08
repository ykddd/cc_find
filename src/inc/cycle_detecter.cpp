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

#define SkipTsBack(last_index, cur_index, node)               \
    for (cur_index = backward_edge_index[node];               \
         cur_index < backward_edge_index[node + 1];           \
         ++cur_index) {                                       \
        IfTrueThenDo(                                         \
            backward_ts[last_index] > backward_ts[cur_index], \
            break;                                            \
        );                                                    \
}

#define SkipAmt(last_index, cur_index)                                    \
    if (forward_value[last_index] * 9 > forward_value[cur_index] * 10 ||  \
        forward_value[last_index] * 11 < forward_value[cur_index] * 10) { \
            continue;                                                     \
    }

#define SkipAmtBack(last_index, cur_index)                                  \
    if (backward_value[cur_index] * 9 > backward_value[last_index] * 10 ||  \
        backward_value[cur_index] * 11 < backward_value[last_index] * 10) { \
            continue;                                                       \
    }



#define SkipDupliNode(cond)       \
    IfTrueThenDo(cond, continue;) \


void CycleDetecter::SortTransfer() {
    PerfThis(__FUNCTION__);
    tbb::parallel_sort(forward_trans, forward_trans + edge_num, Transfer::cmp_transfer);
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
                              forward_src[i] = forward_trans[i].src_id;
                              forward_dst[i] = forward_trans[i].dst_id;
                              forward_ts[i] = forward_trans[i].time_stamp;
                              forward_value[i] = forward_trans[i].amount;
                          }
                      });
//    for (uint32_t i = 0 ; i < edge_num; ++i) {
//        std::cout << forward_src[i] << " ";
//    }
//    std::cout << std::endl;
//    for (uint32_t i = 0 ; i < edge_num; ++i) {
//        std::cout << forward_dst[i] << " ";
//    }
//    std::cout << std::endl;
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
}

void CycleDetecter::MakeEdgeIndex() {
    PerfThis(__FUNCTION__);

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
//    for (uint32_t i = 0 ; i < account_num + 1; ++i) {
//        std::cout << forward_edge_index[i] << " ";
//    }
//    std::cout << std::endl;
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
//    for (uint32_t i = 0 ; i < account_num + 1; ++i) {
//        std::cout << backward_edge_index[i] << " ";
//    }
//    std::cout << std::endl;
}

void CycleDetecter::FindCycle() {
    PerfThis(__FUNCTION__);
//    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, account_num),
//                      [&](tbb::blocked_range<uint32_t> r) {
//                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
//                              NativeSixFor(i);
//                          }
//                      });

    tbb::parallel_for(tbb::blocked_range<uint32_t>(0, account_num),
                      [&](tbb::blocked_range<uint32_t> r) {
                          auto back_rec = new BackRec[10000];
                          auto in_back = new bool[account_num]{};
                          auto back_record_index = new uint32_t[account_num + 1];
                          uint32_t back_recode_num = 0;
                          for (uint32_t i = r.begin(); i < r.end(); ++i) {
                              BackFindThree(i, back_rec, in_back, back_record_index, back_recode_num);
                              ForwardFindThree(i, back_rec, in_back, back_record_index);
                          }
                          delete []back_rec;
                          delete []in_back;
                          delete []back_record_index;
                      });

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
    char local_res[300];
    if (cur_node % 10000 == 0) {
        std::cout << "cur node: " << cur_node << std::endl;
    }
    uint32_t node_1, node_2, node_3, node_4, node_5;
    uint32_t j, k, m, n, u;
    std::array<uint32_t, 7> res_index{cur_node};
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
                             res_index[3] = k;

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
                                 res_index[4] = m;

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
                                     res_index[5] = n;
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
                                         res_index[6] = u;
                                         ExportRes(res_index, 6, local_res);)
                        }
                    }
                }
            }
        }
    }
}

void CycleDetecter::BackFindThree(uint32_t cur_node, BackRec* back_rec,
                                  bool *in_back, uint32_t *back_record_index,
                                  uint32_t& back_recode_num) {
    uint32_t node_1, node_2, node_3;
    uint32_t i, j, k;
    for(i = 0 ; i < back_recode_num; ++i){
        in_back[back_rec[i].back_node] = false;
    }
    back_recode_num = 0;
    for (i = backward_edge_index[cur_node]; i < backward_edge_index[cur_node + 1]; ++i) {
        node_1 = backward_dst[i];
        SkipTsBack(i, j, node_1);
        for (j = backward_edge_index[node_1]; j < backward_edge_index[node_1 + 1]; ++j) {
            if (backward_ts[j] > backward_ts[i]) {
                continue;
            }
            SkipAmtBack(i, j);
            node_2 = backward_dst[j];
            SkipDupliNode(node_2 == cur_node);
//            SkipTsBack(j, k, node_2);
            for (k = backward_edge_index[node_2]; k < backward_edge_index[node_2 + 1]; ++k) {
                if (backward_ts[k] > backward_ts[j]) {
                    continue;
                }
                SkipAmtBack(j, k);
                node_3 = backward_dst[k];
                SkipDupliNode(node_3 == node_1);
                back_rec[back_recode_num++] = BackRec(node_3, k, j, i);
                in_back[node_3] = true;
            }
        }
    }
    if(back_recode_num == 0) {
        return;
    }
    std::sort(back_rec, back_rec + back_recode_num, BackRec::CmpBackRec);

    back_record_index[back_rec[0].back_node] = 0;

    for(uint32_t idx = 1; idx < back_recode_num; ++idx) {
        if(back_rec[idx-1].back_node == back_rec[idx].back_node)
            continue;
        back_record_index[back_rec[idx-1].back_node + 1] = idx;
        back_record_index[back_rec[idx].back_node] = idx;
    }

    back_record_index[back_rec[back_recode_num - 1].back_node + 1] = back_recode_num;
}

void CycleDetecter::ForwardFindThree(uint32_t cur_node, BackRec *back_rec, bool *in_back, uint32_t *back_record_index) {
    char local_res[300];
//    if (cur_node % 10000 == 0) {
//        std::cout << "cur node: " << cur_node << std::endl;
//    }
    uint32_t node_1, node_2, node_3, node_4, node_5;
    uint32_t back2, back3, back4;
    uint32_t j, k, m, n, u;
    std::array<uint32_t, 7> res_index{cur_node};
    for (uint32_t i = forward_edge_index[cur_node]; i < forward_edge_index[cur_node + 1]; ++i) {
        node_1 = forward_dst[i];
        if (in_back[node_1]) {
            for (j = back_record_index[node_1]; j < back_record_index[node_1 + 1]; ++j) {
                BackRec& tmp_back = back_rec[j];
                if (forward_ts[i] > backward_ts[tmp_back.b1_idx]) {
                    continue;
                }
                if (forward_value[i] * 9 > backward_value[tmp_back.b1_idx] * 10 ||
                    forward_value[i] * 11 < backward_value[tmp_back.b1_idx] * 10) {
                    continue;
                }
                res_index[1] = i;
                res_index[2] = tmp_back.b1_idx;
                res_index[3] = tmp_back.b2_idx;
                res_index[4] = tmp_back.b3_idx;
                ExportResWithBackRec(res_index, 4, local_res);
//                ExportRes(res_index, 4, local_res);
                ++cycyle_num_len_4;
            }
        }
        SkipTs(i, j, node_1);
        for (; j < forward_edge_index[node_1 + 1]; ++j) {
            SkipAmt(i, j);
            node_2 = forward_dst[j];
            SkipDupliNode(node_2 == cur_node);

            if (in_back[node_2]) {
                for (k = back_record_index[node_2]; k < back_record_index[node_2 + 1]; ++k) {
                    BackRec& tmp_back = back_rec[k];
                    if (forward_ts[j] > backward_ts[tmp_back.b1_idx]) {
                        continue;
                    }

                    if (forward_value[j] * 9 > backward_value[tmp_back.b1_idx] * 10 ||
                        forward_value[j] * 11 < backward_value[tmp_back.b1_idx] * 10) {
                        continue;
                    }
                    if (backward_dst[tmp_back.b2_idx] == node_1 ||
                        backward_dst[tmp_back.b3_idx] == node_1) {
                        continue;
                    }
                    res_index[1] = i;
                    res_index[2] = j;
                    res_index[3] = tmp_back.b1_idx;
                    res_index[4] = tmp_back.b2_idx;
                    res_index[5] = tmp_back.b3_idx;
                    ExportResWithBackRec(res_index, 5, local_res);
//                ExportRes(res_index, 4, local_res);
                    ++cycyle_num_len_5;
                }
            }

            SkipTs(j, k, node_2);
            for (; k < forward_edge_index[node_2 + 1]; ++k) {
                SkipAmt(j, k);
                node_3 = forward_dst[k];
                IfTrueThenDo(node_3 == cur_node,
                             res_index[1] = i;
                             res_index[2] = j;
                             res_index[3] = k;
                             ExportRes(res_index, 3, local_res);
                             ++cycyle_num_len_3;
                             continue;)
                SkipDupliNode(node_3 == node_1);
                if (in_back[node_3]) {
                    for (m = back_record_index[node_3]; m < back_record_index[node_3 + 1]; ++m) {
                        BackRec& tmp_back = back_rec[m];
                        if (forward_ts[k] > backward_ts[tmp_back.b1_idx]) {
                            continue;
                        }
                        if (forward_value[k] * 9 > backward_value[tmp_back.b1_idx] * 10 ||
                            forward_value[k] * 11 < backward_value[tmp_back.b1_idx] * 10) {
                            continue;
                        }
                        back2 = backward_dst[tmp_back.b2_idx];
                        back3 = backward_dst[tmp_back.b3_idx];
                        if (back2 == node_1 || back3 == node_1 ||
                            back2 == node_2 || back3 == node_2) {
                            continue;
                        }
                        res_index[1] = i;
                        res_index[2] = j;
                        res_index[3] = k;
                        res_index[4] = tmp_back.b1_idx;
                        res_index[5] = tmp_back.b2_idx;
                        res_index[6] = tmp_back.b3_idx;
                        ExportResWithBackRec(res_index, 6, local_res);
//                ExportRes(res_index, 4, local_res);
                        ++cycyle_num_len_6;
                    }
                }
            }
        }
    }
}


void CycleDetecter::ExportRes(const std::array<uint32_t, 7> &res_index,
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

void CycleDetecter::ExportResImpl(const std::array<uint32_t, 7> &res_index, char *tmp_buffer,
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

void CycleDetecter::ExportResWithBackRec(const std::array<uint32_t, 7> &res_index,
                                         uint32_t cycle_len, char *tmp_buffer) {
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

    backward_src = new uint32_t[edge_num];
    backward_dst = new uint32_t[edge_num];
    backward_ts = new int64_t[edge_num];
    backward_value = new uint32_t[edge_num];
    backward_edge_index = new uint32_t[account_num + 2];

    SortTransfer();
    FlattenTrans();
    MakeEdgeIndex();


    SortBackTransfer();
    FlattenBackTrans();
    MakeBackEdgeIndex();

    res_data = new char[335620 * 300];
    FindCycle();
    ExportResToFile();
}









