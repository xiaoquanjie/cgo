//
// Created by xiaoqj on 2024/4/1.
//

#include "rmdbclient/rmdbclient.h"
#include "cgo.h"
#include <iostream>

void set_data(rmdbclient::RmdbClientPtr& cli) {
    auto ret = cli->SetString("TestTable", {
            {"id", "123456"},
            {"name", "string"}
    }, "this is a rmdb test");

    if (M_RMDB_OK(ret)) {
        std::cout << "set data ok\n";
    }
    else if (M_RMDB_IOCODE(ret) != 0) {
        std::cout << "failed to setstring, iocode:" << M_RMDB_IOCODE(ret) << "\n";
    } else {
        std::cout << "failed to setstring:" << M_RMDB_MSG(ret) << "\n";
    }
}

void get_data(rmdbclient::RmdbClientPtr& cli) {
    std::string val;
    auto ret = cli->GetString("TestTable", {
            {"id", "123456"},
            {"name", "string"}
    }, val);

    if (M_RMDB_OK(ret)) {
        std::cout << "get data:" << val << "\n";
    } else if (M_RMDB_IOCODE(ret) != 0) {
        std::cout << "failed to getstring, iocode:" << M_RMDB_IOCODE(ret) << "\n";
    } else {
        std::cout << "failed to getstring:" << M_RMDB_MSG(ret) << "\n";
    }
}

void delete_data(rmdbclient::RmdbClientPtr& cli) {
    cli->DelData("TestTable", {
            {"id", "123456"},
            {"name", "string"}
    });
}

int main() {
    // 事先创建好客户端.
    rmdbclient::CreateClient(1, "192.168.102.26:50001/rmdb");
    cgo::WaitGroup wg;

    for (int i = 0; i < 1; i++) {
        wg.Add(1);
        go [&wg] {
            auto cli = rmdbclient::GetClient(1);
            if (!cli) {
                std::cout << "client error\n";
                return;
            }

            set_data(cli);
            get_data(cli);
            delete_data(cli);

            wg.Done();
        };
    }

    wg.Wait();
    return 0;
}