// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <cgo/thirdparty/otl/otl.h>
#include "common/helloworld.grpc.pb.h"

class GreeterClient : public cogrpc::Client<helloworld::Greeter> {
public:
    GRPC_CLIENT_UNARY_METHOD(SayHello, helloworld::HelloRequest, helloworld::HelloReply);

    GRPC_CLIENT_UNARY_METHOD(GetName, helloworld::FamilyRequest, helloworld::FamilyResponse);

    GRPC_CLIENT_BS_METHOD(ListName, helloworld::FamilyRequest, helloworld::FamilyResponse);

    GRPC_CLIENT_CS_METHOD(ClientStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);

    GRPC_CLIENT_SS_METHOD(ServerStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);

};

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
        std::cout << "SayHello:" << req->ShortDebugString() << "\n";

        for (auto& kv : ctx->client_metadata()) {
            std::cout << "key:" << std::string(kv.first.data(), kv.first.size()) << " value:" << std::string(kv.second.data(), kv.second.size()) << "\n";
        }
        ctx->AddInitialMetadata("servermd1", "serverd1");
        ctx->AddTrailingMetadata("servermd2", "serverd2");
        return ::grpc::Status::OK;
        GreeterClient client;
        client.AddInterceptor<otl::DefaultGrpcClientInterceptor>();
        client.Bind("127.0.0.1:8080", "");

        auto clientCtx = cogrpc::FromServerContext(ctx);
        clientCtx->AddMetadata("user_id2", "fdskfldsfsd");
        helloworld::FamilyRequest freq;
        helloworld::FamilyResponse frsp;
        client.GetName(clientCtx, freq, &frsp);

        return ::grpc::Status::OK;
    }

    ::grpc::Status GetName(::grpc::ServerContext *ctx, const helloworld::FamilyRequest *req, helloworld::FamilyResponse *rsp) {
        std::cout << "getname" << "\n";
        for (auto& kv : ctx->client_metadata()) {
            std::cout << "key:" << std::string(kv.first.data(), kv.first.size()) << " value:" << std::string(kv.second.data(), kv.second.size()) << "\n";
        }
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
    setbuf(stdout, nullptr);
    auto exporter = otl::CreateConsoleExporter();
    otl::ExporterContainer container;
    container.push_back(std::move(exporter));
    //container.push_back(otl::CreateHttpExporter("http://192.168.102.41:4318/v1/traces"));
    otl::Init(argc, argv, container);

    cogrpc::DefSrvBuilder()->AddListeningPort("0.0.0.0:8080");
    cogrpc::DefSrvBuilder()->RegisterService<GreeterServer>();
    //cogrpc::DefSrvBuilder()->AddInterceptor<otl::DefaultGrpcServerInterceptor>();
    cogrpc::DefSrvBuilder()->Run();

    while (true) {
        msleep(100);
    }

    return 0;
}