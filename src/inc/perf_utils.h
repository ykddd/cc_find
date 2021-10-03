//
// Created by ykddd on 2021/9/30.
//
#include <chrono>
#include <string>
#include <iostream>

#ifndef CYCLE_DETECT_PERF_UTILS_H
#define CYCLE_DETECT_PERF_UTILS_H

#define TimeNow() std::chrono::steady_clock::now()
#define PerfThis(name) PerfUtils::PerfRaii per_raii{name}
class PerfUtils {
public:
    class PerfRaii {
    public:
        PerfRaii(std::string name) : name(name) {
            birth_day = TimeNow();
        }

        ~PerfRaii() {
            std::cout << "[PERF_INFO] " << name << " cost: "
                      << std::chrono::duration<double, std::milli>(TimeNow() - birth_day).count()
                      << " ms." << std::endl;
        }

    private:
        std::string name;
        std::chrono::steady_clock::time_point birth_day;
    };
};

#endif //CYCLE_DETECT_PERF_UTILS_H
