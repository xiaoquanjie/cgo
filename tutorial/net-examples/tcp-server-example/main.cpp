//
// Created by xiaoqj on 2024/4/1.
//

#include <cgo/thirdparty/conet/conet.h>
#include <iostream>

int main() {
    auto listener = conet::ListenTcp("tcp", "0.0.0.0", 8080);
    if (!listener) {
        std::cout << "listen error\n";
    }

    std::cout << "listen in 8080\n";
    cgo::WaitGroup wg;
    wg.Add(1);

    go [listener, &wg] {
        while (true) {
            auto conn = listener->Accept();
            if (!conn) {
                std::cout << "accept error\n";
                wg.Done();
                break;
            }

            go [conn] {
                auto buf = new char[1024];
                int cnt = 0;
                buf[cnt] = 0;
                while (true) {
                    auto tmp = conn->Read(buf + cnt, 1024 - cnt);
                    if (tmp <= 0) {
                        break;
                    }

                    cnt += tmp;
                    for (int idx = 0; idx < tmp; idx++) {
                        std::cout << buf[cnt - tmp + idx];
                        if (buf[cnt - tmp + idx] == '\n') {
                            conn->Write("server echo:", 12);
                            conn->Write(buf, cnt);
                            cnt = 0;
                            buf[cnt] = 0;
                            break;
                        }
                    }
                }
                delete []buf;
                delete conn;
            };
        }
    };

    wg.Wait();
    delete listener;
    return 0;
}