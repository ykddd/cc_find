//
// Created by ykddd on 2021/9/30.
//
#include <chrono>
#include <string>
#include <map>
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
            auto count = std::chrono::duration<double, std::milli>(TimeNow() - birth_day).count();
            if (name_to_time.find(name) == name_to_time.end()) {
                name_to_time[name] = count;
            } else {
                name_to_time[name] += count;
            }
//            std::cout << "[PERF_INFO] " << name << " cost: "
//                      << std::chrono::duration<double, std::milli>(TimeNow() - birth_day).count()
//                      << " ms." << std::endl;
        }

        static void Report(){
            for (auto &item : name_to_time) {
                std::cout << "[PERF_INFO] ";
                std::cout << item.first;
                std::cout << " cost: ";
                std::cout << item.second << " ms." << std::endl;
            }
        }

    private:
        std::string name;
        std::chrono::steady_clock::time_point birth_day;
        static std::map<std::string, double> name_to_time;
    };
};



#endif //CYCLE_DETECT_PERF_UTILS_H
