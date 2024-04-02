//
// Created by xiaoqj on 2024/4/1.
//

#include <cgo/thirdparty/conet/conet.h>
#include <iostream>

int main() {
    cgo::WaitGroup wg;
    wg.Add(1);

    go [&wg] {
        do {
            auto conn = conet::DialTcp("tcp", "127.0.0.1", 8080);
            if (!conn) {
                std::cout << "dial error:" << conet::Errno() << "\n";
                break;
            }

            for (;;) {
                std::string input;
                std::cin >> input;
                if (input == "end") {
                    break;
                }
                if (conn->Write(input.c_str(), (int)input.length()) <= 0) {
                    break;
                }
                for (char c : input) {
                    if (c == '\n') {
                        auto buf = new char[1024];
                        int cnt = 0;
                        while ((cnt = conn->Read(buf, 1024)) > 0) {
                            buf[cnt] = 0;
                            std::cout << buf;
                            for (decltype(cnt) idx = 0; idx < cnt; idx++) {
                                if (buf[idx] == '\n') {
                                    break;
                                }
                            }
                        }
                        delete []buf;
                        break;
                    }
                }
            }
            // release connection object
            delete conn;
        } while (false);
        wg.Done();
    };

    wg.Wait();
    return 0;
}