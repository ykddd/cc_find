//
// Created by ykddd on 2021/9/25.
//

#ifndef CYCLE_DETECT_TRANSFER_H
#define CYCLE_DETECT_TRANSFER_H

//src.id:INT64, dst.id:INT64, timestamp:INT64, amount:double
#include <cstdlib>

using EdgeNumType = uint32_t;
using IDType = uint32_t;
using TSType = uint64_t;
using AmtType = uint32_t;

struct Transfer {
public:
    Transfer() = default;

    Transfer(AmtType amt, IDType src, uint64_t dst, TSType time) :
            amount(amt), src_id(src), dst_id(dst), time_stamp(time) {};

    static bool cmp_transfer(const Transfer &lhs, const Transfer &rhs) {
        return (lhs.src_id < rhs.src_id) ||
               (lhs.src_id == rhs.src_id && lhs.time_stamp < rhs.time_stamp);
    }


    static bool cmp_transfer_ts(const Transfer &lhs, const Transfer &rhs) {
        return lhs.time_stamp < rhs.time_stamp;
    }

    static int cmp_back_trans(const Transfer &lhs, const Transfer &rhs) {
        return (lhs.dst_id < rhs.dst_id) ||
               (lhs.dst_id == rhs.dst_id && lhs.time_stamp < rhs.time_stamp);
    }

    AmtType amount;
    IDType src_id;
    uint32_t dst_id;
    TSType time_stamp;
};

struct CmpTransferInQue {
    bool operator()(Transfer& lhs, Transfer &rhs) {
        return (lhs.src_id < rhs.src_id) ||
               (lhs.src_id == rhs.src_id && lhs.time_stamp < rhs.time_stamp);
    }
};

#endif //CYCLE_DETECT_TRANSFER_H
