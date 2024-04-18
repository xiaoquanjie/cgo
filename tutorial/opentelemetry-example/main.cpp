// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <cgo/thirdparty/otl/otl.h>
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
        std::cout << req->ShortDebugString() << "\n";
        return ::grpc::Status::OK;
    }

    ::grpc::Status GetName(::grpc::ServerContext *ctx, const helloworld::FamilyRequest *req, helloworld::FamilyResponse *rsp) {
        return ::grpc::Status::OK;
    }

    ::grpc::Status ListName(::grpc::ServerContext *ctx, GRPC_SRV_RW(helloworld::FamilyRequest, helloworld::FamilyResponse) *rw) {
        return ::grpc::Status::OK;
    }

    ::grpc::Status ClientStreamSayHello(::grpc::ServerContext *ctx, GRPC_SRV_READER(helloworld::HelloRequest, helloworld::HelloReply) *reader, helloworld::HelloReply *rsp) {
        return ::grpc::Status::OK;
    }

    ::grpc::Status ServerStreamSayHello(::grpc::ServerContext *ctx, const helloworld::HelloRequest *req, GRPC_SRV_WRITER(helloworld::HelloRequest, helloworld::HelloReply) *writer) {
        return ::grpc::Status::OK;
    }

};

int main(int argc, char **argv) {
    auto exporter = otl::CreateConsoleExporter();
    otl::ExporterContainer container;
    container.push_back(std::move(exporter));
    //container.push_back(otl::CreateHttpExporter("http://192.168.102.41:4318/v1/traces"));
    otl::Init(argc, argv, container);

    cogrpc::DefSrvBuilder()->AddListeningPort("0.0.0.0:8080");
    cogrpc::DefSrvBuilder()->RegisterService<GreeterServer>();
    cogrpc::DefSrvBuilder()->AddInterceptor<otl::DefaultGrpcServerInterceptor>();
    cogrpc::DefSrvBuilder()->Run();

    while (true) {
        msleep(100);
    }

    return 0;
}