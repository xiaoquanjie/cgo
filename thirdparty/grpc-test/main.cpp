//
// Created by xiaoqj on 2023/12/26.
//

#include "./helloworld.grpc.pb.h"
#include "../../cgo/cgo.h"
#include "co_grpc/co_grpc.h"
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

class GreeterClient : public cogrpc::Client<helloworld::Greeter> {
public:
    GRPC_CLIENT_UNARY_METHOD(SayHello, helloworld::HelloRequest, helloworld::HelloReply);

    GRPC_CLIENT_CS_METHOD(ClientStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);

    GRPC_CLIENT_SS_METHOD(ServerStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);

    GRPC_CLIENT_BS_METHOD(ListName, helloworld::FamilyRequest, helloworld::FamilyResponse);

};

class GreeterServer : public cogrpc::Server<helloworld::Greeter> {
public:
    void InitMethod() override {
        GRPC_SRV_UNARY_METHOD(SayHello, helloworld::HelloRequest, helloworld::HelloReply);
        GRPC_SRV_CS_METHOD(ClientStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);
        GRPC_SRV_SS_METHOD(ServerStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);
        GRPC_SRV_BS_METHOD(ListName, helloworld::FamilyRequest, helloworld::FamilyResponse);
    }

    // 一元类
    ::grpc::Status SayHello(::grpc::ServerContext* ctx,
                            const helloworld::HelloRequest* req,
                            helloworld::HelloReply* rsp) {
        rsp->set_message("server.SayHello: hello, this is server");
        return ::grpc::Status::OK;
    }

    // 客户端流
    ::grpc::Status ClientStreamSayHello(::grpc::ServerContext* ctx,
                                        GRPC_SRV_READER(helloworld::HelloRequest, helloworld::HelloReply) *reader,
                                        helloworld::HelloReply* rsp) {
        helloworld::HelloRequest req;
        while (reader->Read(&req)) {
            print_withtime(std::string("server recv:") + req.ShortDebugString());
        }

        rsp->set_message("server.ClientStreamSayHello: hello, this is server");
        return ::grpc::Status::OK;
    }

    //  服务器端流
    ::grpc::Status ServerStreamSayHello(::grpc::ServerContext* ctx,
                                        const helloworld::HelloRequest* req,
                                        GRPC_SRV_WRITER(helloworld::HelloRequest, helloworld::HelloReply) *writer) {
        print_withtime(std::string("server recv:") + req->ShortDebugString());
        for (int i = 0; i < 10; i++) {
            helloworld::HelloReply rsp;
            rsp.set_message("server.ServerStreamSayHello send: this is server");
            writer->Write(rsp);
        }

        return ::grpc::Status::OK;
    }

    // 双流
    ::grpc::Status ListName(::grpc::ServerContext* ctx,
                            GRPC_SRV_RW(helloworld::FamilyRequest, helloworld::FamilyResponse) *rw) {
        cgo::WaitGroup wg;
        wg.Add(2);

        go [rw, &wg] {
            for (int i = 0; i < 10; i++) {
                helloworld::FamilyResponse rsp;
                rsp.set_name("server");
                rw->Write(rsp);
            }
            wg.Done();
        };

        go [rw, &wg] {
            for (int i = 0; i< 10; i++) {
                helloworld::FamilyRequest req;
                if (!rw->Read(&req)) {
                    break;
                }
                print_withtime("req:" + req.ShortDebugString());

                helloworld::FamilyResponse rsp;
                rsp.set_name("server");
                rw->Write(rsp);
            }

            wg.Done();
        };

        wg.Wait();
        print_withtime("server.ListName: over");
        return ::grpc::Status::OK;
    }
};

void unary_test() {
    go [] {
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
    };
}

void client_stream_test() {
    go [] {
        GreeterClient client;
        client.Bind("0.0.0.0:50051", "");
        auto ctx = cogrpc::MakeContext();
        helloworld::HelloReply rsp;
        auto writer = client.ClientStreamSayHello(ctx, &rsp);
        if (!writer) {
            print_withtime("get writer error");
            return;
        }

        for (int i = 0; i < 10; i++) {
            helloworld::HelloRequest req;
            req.set_name("jack");
            if (!writer->Write(req)) {
                print_withtime("write error");
                break;
            }
        }

        auto status = writer->Finish();
        if (GRPC_OK(status)) {
            print_withtime(rsp.message().c_str());
        } else {
            print_withtime("get write result error");
        }
    };
}

void server_stream_test() {
    go [] {
        GreeterClient client;
        client.Bind("0.0.0.0:50051", "");

        helloworld::HelloRequest req;
        req.set_name("jack");
        req.set_count(2);

        auto ctx = cogrpc::MakeContext();
        auto reader = client.ServerStreamSayHello(ctx, req);
        if (!reader) {
            print_withtime("get reader error");
            return;
        }

        helloworld::HelloReply rsp;
        while (reader->Read(&rsp)) {
            print_withtime(rsp.message().c_str());
        }
    };
}

void double_stream_test() {
    go [] {
        GreeterClient client;
        client.Bind("0.0.0.0:50051", "");

        auto rw = client.ListName(nullptr);
        if (!rw) {
            print_withtime("get rw error");
            return;
        }

        //return;

        go [rw]() {
            for (;;) {
                helloworld::FamilyRequest req;
                req.set_family("jack family");
                if (!rw->Write(req)) {
                    print_withtime("client.ListName write error");
                    break;
                }
                gowait(1000);
            }
        };

        go [rw]() {
            helloworld::FamilyResponse rsp;
            while (rw->Read(&rsp)) {
                print_withtime("rsp:" + rsp.ShortDebugString());
            }

            print_withtime("client.ListName read error");
        };
    };
}

void grpc_listen() {
    cogrpc::DefSrvBuilder()->RegisterService<GreeterServer>();
    cogrpc::DefSrvBuilder()->AddListeningPort("0.0.0.0:50051");
    cogrpc::DefSrvBuilder()->Run();
}

int main() {
    grpc_listen();

    //msleep(1000);

//    unary_test();
//    msleep(1000*2);
//
    client_stream_test();
    msleep(1000*2);
//
//    server_stream_test();
//    msleep(1000*2);

    //double_stream_test();

    while (true) {
        msleep(1000);
    }
    return 0;
}
