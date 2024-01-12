//
// Created by xiaoqj on 2024/1/9.
//

#include <iostream>
#include "cgo/cgo.h"
#include "rmdbclient/rmdbclient.h"

int main() {
    // 事先创建好客户端
    rmdbclient::CreateClient(1, "192.168.102.26:50001/rmdb");

    for (int i = 0; i < 10; i++) {
        go []() {
            while (true) {
                auto ptr = rmdbclient::GetClient(1);
                if (ptr) {
                    std::string val;
                    auto ret = ptr->GetString("TestTable", {
                            {"id", "123456"},
                            {"name", "string"}
                    }, val);

                    if (M_RMDB_OK(ret)) {
                        std::cout << "data:" << val << "\n";
                    } else if (M_RMDB_IOCODE(ret) != 0) {
                        std::cout << "failed to getkv, iocode:" << M_RMDB_IOCODE(ret) << "\n";
                    } else {
                        std::cout << "failed to getkv:" << M_RMDB_CODE(ret) << "\n";
                    }
                }

                gosleep(500);
            }
        };
    }

    while (true) {
        msleep(1);
    }
    return 0;
}