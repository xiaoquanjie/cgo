//
// Created by xiaoqj on 2024/1/9.
//

#include <iostream>
#include <string.h>
#include "co_net/co_net.h"

void tcp_listen() {
    auto li = co_net::ListenTcp("tcp", "0.0.0.0", 50053);
    if (!li) {
        std::cout << "bind error\n";
        return;
    }

    go [li] {
        while (true) {
            auto conn = li->Accept();
            if (!conn) {
                std::cout << "accept error\n";
                break;
            }
            std::cout << "new connect:" << conn->LocalAddr().String() << "|" << conn->RemoteAddr().String() << "\n";
            char buf[1024] = {0};
            auto cnt = conn->Read(buf, 1024);
            conn->Write(buf, cnt);
            conn->Close();
            delete conn;
        }
        delete li;
    };
}

void tcp_client() {
    go [] {
        while (true) {
            auto conn = co_net::DialTcp("tcp", "127.0.0.1", 50053);
            if (!conn) {
                std::cout << "dial error\n";
                return;
            }

            std::string val = "hello tcp";
            if (conn->Write(val.c_str(), val.size()) <= 0) {
                break;
            }
            char buf[1024] = {0};
            if (conn->Read(buf, 1024) <= 0) {
                break;
            }
            std::cout << buf << "\n";

            delete conn;
            gosleep(1000*2);
        }
    };
}

void udp_listen() {
    auto conn = co_net::ListenUdp("udp", "0.0.0.0", 50052);
    if (!conn) {
        std::cout << "bind error\n";
        return;
    }

    go [conn] {
        while (true) {
            co_net::UDPAddr caddr = co_net::EmptyUDPAddr();
            char buf[1024] = {0};
            auto cnt = conn->Read(buf, 1024, &caddr);
            conn->Write(buf, cnt, caddr);
        }
        delete conn;
    };
}

void udp_client() {
    go [] {
        auto conn = co_net::DialUdp("udp");
        auto addr = co_net::ResolveUDPAddr("udp", "127.0.0.1", 50052);
        while (true) {
            std::string val = "hello udp";
            conn->Write(val.c_str(), val.length(), addr);
            char buf[1024] = {0};
            co_net::UDPAddr caddr = co_net::EmptyUDPAddr();
            auto cnt = conn->Read(buf, 1024, &caddr);
            std::cout << buf << "\n";
            gosleep(1000*2);
        }
        delete conn;
    };
}

void http_listen() {
    co_net::ListenHttpAndServe("tcp", "0.0.0.0", 50053, [](co_net::HttpResponse* rsp, co_net::HttpRequest* req) {
//        std::cout << req->Method() << "\n";
//        std::cout << req->Url() << "\n";
//        for (auto& kv : req->Header()) {
//            std::cout << kv.first << " " << kv.second << "\n";
//        }
//        std::cout << req->Body() << "\n";

        const char* resp = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!";
        rsp->Write(resp, strlen(resp));
    });
}

int main() {
    //cgoprocs(1);

//    tcp_listen();
//    tcp_client();
//
//    udp_listen();
//    udp_client();

    http_listen();
    cgo::cgo_print_debug_info();

    while (true) {
        msleep(10);
    }
    return 0;
}