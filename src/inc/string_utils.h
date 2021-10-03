//
// Created by ykddd on 2021/10/2.
//

#ifndef CYCLEDETECT_STRING_UTILS_H
#define CYCLEDETECT_STRING_UTILS_H
namespace StrUtils{
#include <stdint.h>

#if 0
    // 1 char at a time

#define W(N, I) b[N] = char(I) + '0'
#define A(N) t = (uint64_t(1) << (32 + N / 5 * N * 53 / 16)) / uint32_t(1e##N) + 1 - N / 9, t *= u, t >>= N / 5 * N * 53 / 16, t += N / 5 * 4, W(0, t >> 32)
#define D(N) t = uint64_t(10) * uint32_t(t), W(N, t >> 32)

#define L0 W(0, u)
#define L1 A(1), D(1)
#define L2 A(2), D(1), D(2)
#define L3 A(3), D(1), D(2), D(3)
#define L4 A(4), D(1), D(2), D(3), D(4)
#define L5 A(5), D(1), D(2), D(3), D(4), D(5)
#define L6 A(6), D(1), D(2), D(3), D(4), D(5), D(6)
#define L7 A(7), D(1), D(2), D(3), D(4), D(5), D(6), D(7)
#define L8 A(8), D(1), D(2), D(3), D(4), D(5), D(6), D(7), D(8)
#define L9 A(9), D(1), D(2), D(3), D(4), D(5), D(6), D(7), D(8), D(9)

#else
// 2 chars at a time

    struct pair { char t, o; };
#define P(T) T, '0',  T, '1', T, '2', T, '3', T, '4', T, '5', T, '6', T, '7', T, '8', T, '9'
    static const pair s_pairs[] = { P('0'), P('1'), P('2'), P('3'), P('4'), P('5'), P('6'), P('7'), P('8'), P('9') };

#define W(N, I) *(pair*)&b[N] = s_pairs[I]
#define A(N) t = (uint64_t(1) << (32 + N / 5 * N * 53 / 16)) / uint32_t(1e##N) + 1 + N/6 - N/8, t *= u, t >>= N / 5 * N * 53 / 16, t += N / 6 * 4, W(0, t >> 32)
#define S(N) b[N] = char(uint64_t(10) * uint32_t(t) >> 32) + '0'
#define D(N) t = uint64_t(100) * uint32_t(t), W(N, t >> 32)

#define L0 b[0] = char(u) + '0'
#define L1 W(0, u)
#define L2 A(1), S(2)
#define L3 A(2), D(2)
#define L4 A(3), D(2), S(4)
#define L5 A(4), D(2), D(4)
#define L6 A(5), D(2), D(4), S(6)
#define L7 A(6), D(2), D(4), D(6)
#define L8 A(7), D(2), D(4), D(6), S(8)
#define L9 A(8), D(2), D(4), D(6), D(8)

#endif

#define LN(N) (L##N, b += N + 1, length += N + 1)
#define LZ LN
// if you want to '\0' terminate
//#define LZ(N) &(L##N, b[N + 1] = '\0')

#define LG(F) (u<100 ? u<10 ? F(0) : F(1) : u<1000000 ? u<10000 ? u<1000 ? F(2) : F(3) : u<100000 ? F(4) : F(5) : u<100000000 ? u<10000000 ? F(6) : F(7) : u<1000000000 ? F(8) : F(9))
    uint16_t u64toa_jeaiii(uint64_t n, char* b)
    {
        uint16_t length = 0;
        uint32_t u;
        uint64_t t;

        if (uint32_t(n >> 32) == 0) {
            u = uint32_t(n);
            LG(LZ);
            return length;
        }

        uint64_t a = n / 100000000;

        if (uint32_t(a >> 32) == 0)
        {
            u = uint32_t(a);
            LG(LN);
        }
        else
        {
            u = uint32_t(a / 100000000);
            LG(LN);
            u = a % 100000000;
            LN(7);
        }

        u = n % 100000000;
        LZ(7);
        return length;
    }
}
#endif //CYCLEDETECT_STRING_UTILS_H
