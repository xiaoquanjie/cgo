//
// Created by xiaoqj on 2024/4/1.
//

#include <cgo/thirdparty/comysql/comysql.h>
#include <iostream>

int main() {
    cgo::WaitGroup wg;
    for (int i = 0; i < 1; i++) {
        wg.Add(1);
        go gostack(1024*32) [&wg] {
            try {
                auto conn = comysql::MysqlPool::GetConnection("192.168.204.61", "root", "root", "test");
                conn.Execute("drop table if exists mysql_test");
                conn.Execute("create table mysql_test(`name` varchar(100))");
                conn.Execute("insert into mysql_test values('meta')");
                conn.Query("select * from mysql_test", [](MYSQL_ROW row, unsigned long long row_num, unsigned int col_num, unsigned long long idx) {
                    std::cout << idx << " ";
                    for (unsigned int col = 0; col < col_num; col++) {
                        std::cout << row[col] << " ";
                    }
                    std::cout << "\n";
                });
                conn.Execute("drop table mysql_test");
            } catch (const std::exception& e) {
                std::cout << "error:" << e.what() << "\n";
            }
            wg.Done();
        };
    }

    wg.Wait();
    return 0;
}