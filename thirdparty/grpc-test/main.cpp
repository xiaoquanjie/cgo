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

void client_stream_test() {
    GreeterClient client;
    client.Bind("0.0.0.0:50051", "");

    helloworld::HelloReply rsp;
    auto writer = client.ClientStreamSayHello(nullptr, &rsp);
    if (!writer) {
        print_withtime("get writer error");
        return;
    }

    helloworld::HelloRequest req;
    req.set_name("jack");
    writer->Write(req);

    auto status = writer->Finish();
    if (GRPC_OK(status)) {
        print_withtime(rsp.message().c_str());
    } else {
        print_withtime("write error");
    }
}

void server_stream_test() {
    GreeterClient client;
    client.Bind("0.0.0.0:50051", "");

    helloworld::HelloRequest req;
    req.set_name("jack");
    req.set_count(2);

    auto reader = client.ServerStreamSayHello(nullptr, req);
    if (!reader) {
        print_withtime("get reader error");
        return;
    }

    helloworld::HelloReply rsp;
    while (reader->Read(&rsp)) {
        print_withtime(rsp.message().c_str());
    }

    reader->Finish();
}

void double_stream_test() {
    GreeterClient client;
    client.Bind("0.0.0.0:50051", "");

    auto rw = client.ListName(nullptr);
    if (!rw) {
        print_withtime("get rw error");
        return;
    }

    go gostack(16*1024) [rw]() {
        for (;;) {
            helloworld::FamilyRequest req;
            req.set_family("jack family");
            if (rw->Write(req) < 0) {
                break;
            }
            gowait(1000);
        }
    };

    go gostack(16*1024) [rw]() {
        helloworld::FamilyResponse rsp;
        while (rw->Read(&rsp)) {
            //print_withtime(rsp.ShortDebugString());
        }
        print_withtime("close");
        rw->Close();
    };

    go gostack(16*1024) [rw]() {
        gowait(5000);
        rw->Close();
    };

    //msleep(10000);
}

int main() {
    cgoprocs(2);
    for (int i = 0; i < 100; i++) {
        go gostack(1024*1024) []() {
            //unary_test();
            //client_stream_test();
            //server_stream_test();
            double_stream_test();
        };
    }

    while (true) {
        msleep(1000);
    }
    return 0;
}