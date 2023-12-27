//
// Created by xiaoqj on 2023/12/26.
//

#include "./helloworld.grpc.pb.h"
#include "../../cgo/cgo.h"
#include "../co_grpc/macro.h"
#include <memory>
#include <iostream>
#include <iomanip>
#include <thread>

void print_withtime(const char* msg) {
    static std::mutex mu;
    std::unique_lock<std::mutex> lock(mu);

    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* timeinfo = std::localtime(&now_c);

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

    std::cout << buffer << '.' << std::setfill('0') << std::setw(3) << ms.count() << " ";
    std::cout << std::this_thread::get_id() << " " << msg << "\n";
}

void print_withtime(const std::string& msg) {
    print_withtime(msg.c_str());
}

class GreeterClient : public co_grpc::CoClient<helloworld::Greeter> {
public:
    //typedef std::shared_ptr<::grpc::ClientContext> ClientContextPtr;

    // 一元类
    GRPC_CLIENT_CO_UNARY_METHOD(SayHello, helloworld::HelloRequest, helloworld::HelloReply);

    GRPC_CLIENT_CO_CS_METHOD(ClientStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);

    GRPC_CLIENT_CO_SS_METHOD(ServerStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);

    GRPC_CLIENT_CO_BS_METHOD(ListName, helloworld::FamilyRequest, helloworld::FamilyResponse);
};

void unary_test() {
    GreeterClient client;
    client.Bind("0.0.0.0:50051", "");

    for (int i = 0; i < 10; i++) {
        helloworld::HelloRequest req;
        req.set_name("jack");
        helloworld::HelloReply rsp;

        auto status = client.SayHello(nullptr, req, &rsp);
        if (GRPC_OK(status)) {
            print_withtime(rsp.message().c_str());
        } else {
            print_withtime("error");
            break;
        }
    }
}

int main() {
    for (int i = 0; i < 1000; i++) {
        go gostack(1024*1024) []() {
            unary_test();
        };
    }

    while (true) {
        msleep(1000);
    }
    return 0;
}