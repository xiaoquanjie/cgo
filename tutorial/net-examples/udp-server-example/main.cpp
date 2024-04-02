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
            auto conn = conet::ListenUdp("udp", "0.0.0.0", 8080);
            if (!conn) {
                std::cout << "listen udp error\n";
                break;
            }

            while (true) {
                auto clientAddr = conet::EmptyUDPAddr();
                char buf[1024] = {'s', 'e', 'r', 'v', 'e', 'r', ' ', 'e', 'c', 'h', 'o', ':'};
                auto cnt = conn->Read(buf+12, 1024-12, &clientAddr);
                for (decltype(cnt) idx = 0; idx < cnt; idx++) {
                    std::cout << buf[12+idx];
                }
                std::cout << "\n";
                if (conn->Write(buf, 12+cnt, clientAddr) <= 0) {
                    break;
                }
            }
        } while (false);
        wg.Done();
    };

    wg.Wait();
    return 0;
}