//
// Created by xiaoqj on 2024/1/9.
//

#include <iostream>
#include <string.h>
#include "cgo/cgo.h"
#include "co_mysql/co_mysql.h"

int main() {
    // 为了测试是否成功hook了mysql c driver，把cgo的并发线程限制1个
    cgoprocs(1);

    for (int i = 0; i < 10; i++) {
        go []() {
            try {
                auto c = co_mysql::MysqlPool::GetConnection("192.168.204.61", "root", "root", "test");
                std::cout << c.Execute("update students set name='meta2'") << "\n";

                c.Query("select * from students",
                        [](MYSQL_ROW row, unsigned long long row_num, unsigned int col_num, unsigned long long idx) {
                    std::cout << idx << " ";
                    for (unsigned int col = 0; col < col_num; col++) {
                        std::cout << row[col] << " ";
                    }
                    std::cout << "\n";
                });

            } catch (co_mysql::MysqlException& e) {
                std::cerr << e.What() << "\n";
            }
        };
    }

    // 如果成功hook住了，则gosleep会正常相隔一秒不断的输出
    go [] {
        while (true) {
            std::cout << "gosleep\n";
            gosleep(1000);
        }
    };

    while (true) {
        msleep(1);
    }
    return 0;
}