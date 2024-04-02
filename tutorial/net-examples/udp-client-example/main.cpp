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
            auto conn = conet::DialUdp("udp");
            conet::UDPAddr serverAddr("udp", "127.0.0.1", 8080);

            for (;;) {
                std::string input;
                std::getline(std::cin, input);
                if (input == "end") break;
                if (conn->Write(input.c_str(), (int)input.length(), serverAddr) <= 0) {
                    break;
                }
                char buf[1024] = {0};
                auto caddr = conet::EmptyUDPAddr();
                auto cnt = conn->Read(buf, 1024, &caddr);
                std::cout << buf << "\n";
            }

        } while (false);
        wg.Done();
    };

    wg.Wait();
    return 0;
}