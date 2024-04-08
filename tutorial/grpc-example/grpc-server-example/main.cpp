//
// Created by xiaoqj on 2024/4/2.
//

#include <cgo/thirdparty/cogrpc/cogrpc.h>
#include "common/helloworld.grpc.pb.h"

class GreeterServer : public cogrpc::Server<helloworld::Greeter> {
public:
    void InitMethod() override {
        GRPC_SRV_UNARY_METHOD(SayHello, helloworld::HelloRequest, helloworld::HelloReply);
        GRPC_SRV_UNARY_METHOD(GetName, helloworld::FamilyRequest, helloworld::FamilyResponse);
        GRPC_SRV_BS_METHOD(ListName, helloworld::FamilyRequest, helloworld::FamilyResponse);
        GRPC_SRV_CS_METHOD(ClientStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);
        GRPC_SRV_SS_METHOD(ServerStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);
    }

    ::grpc::Status SayHello(::grpc::ServerContext *ctx, const helloworld::HelloRequest *req, helloworld::HelloReply *rsp) {
        std::cout << "unary SayHello:" << req->ShortDebugString() << "\n";
        rsp->set_message("hello");
        return ::grpc::Status::OK;
    }

    ::grpc::Status GetName(::grpc::ServerContext *ctx, const helloworld::FamilyRequest *req, helloworld::FamilyResponse *rsp) {
        return ::grpc::Status::OK;
    }

    ::grpc::Status ListName(::grpc::ServerContext *ctx, GRPC_SRV_RW(helloworld::FamilyRequest, helloworld::FamilyResponse) *rw) {
        helloworld::FamilyRequest req;
        rw->Read(&req);
        std::cout << "double stream ListName:" << req.ShortDebugString() << "\n";

        helloworld::FamilyResponse rsp;
        rsp.set_name("hello");
        rw->Write(rsp);
        return ::grpc::Status::OK;
    }

    ::grpc::Status ClientStreamSayHello(::grpc::ServerContext *ctx, GRPC_SRV_READER(helloworld::HelloRequest, helloworld::HelloReply) *reader, helloworld::HelloReply *rsp) {
        rsp->set_message("hello");
        helloworld::HelloRequest req;
        reader->Read(&req);
        std::cout << "client stream ClientStreamSayHello:" << req.ShortDebugString() << "\n";
        return ::grpc::Status::OK;
    }

    ::grpc::Status ServerStreamSayHello(::grpc::ServerContext *ctx, const helloworld::HelloRequest *req, GRPC_SRV_WRITER(helloworld::HelloRequest, helloworld::HelloReply) *writer) {
        std::cout << "server stream ServerStreamSayHello:" << req->ShortDebugString() << "\n";
        helloworld::HelloReply rsp;
        rsp.set_message("hello");
        writer->Write(rsp);
        return ::grpc::Status::OK;
    }

};

int main() {
    cogrpc::DefSrvBuilder()->AddListeningPort("0.0.0.0:50052");
    cogrpc::DefSrvBuilder()->RegisterService<GreeterServer>();
    cogrpc::DefSrvBuilder()->Run();

    while (true) {
        msleep(1000);
    }
    return 0;
}