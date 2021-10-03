//
// Created by ykddd on 2021/9/25.
//
#include "cycle_detecter.h"

int main(int argc, char* argv[]) {
//    char trans_file[] = "/Users/ykddd/Desktop/CYCLE_DETECT_2021BDCI/data/test/test.csv";
//    char account_file[] = "/Users/ykddd/Desktop/CYCLE_DETECT_2021BDCI/data/test/account.csv";
//    char trans_file[] = "/Users/ykddd/Desktop/CYCLE_DETECT_2021BDCI/data/scale1/transfer.csv";
//    char account_file[] = "/Users/ykddd/Desktop/CYCLE_DETECT_2021BDCI/data/scale1/account.csv";
//    char trans_file[] = "/Users/ykddd/Desktop/CYCLE_DETECT_2021BDCI/data/scale10/transfer.csv";
//    char account_file[] = "/Users/ykddd/Desktop/CYCLE_DETECT_2021BDCI/data/scale10/account.csv";

    if (argc != 4) {
        std::cerr << "args error!" << std::endl;
        return 0;
    }

    auto account_file = argv[1];
    auto trans_file = argv[2];
    auto res_file = argv[3];

    CycleDetecter cycle_detecter(account_file, trans_file, res_file);
    cycle_detecter.Run();

    return 0;
}