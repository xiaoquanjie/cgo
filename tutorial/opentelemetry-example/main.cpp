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

GreeterClient getClient() {
    static GreeterClient cli;
    if (!cli.Valid()) {
        cli.AddInterceptor<otl::DefaultGrpcClientInterceptor>();
        cli.Bind("127.0.0.1:8080", "");
    }
    return cli;
}

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

        auto cli = getClient();
        auto clientCtx = cogrpc::FromServerContext(ctx);
        helloworld::FamilyRequest freq;
        helloworld::FamilyResponse frsp;
        cli.GetName(clientCtx, freq, &frsp);

        return ::grpc::Status::OK;
    }

    void showTest(const otl::Context &ctx) {
        auto [newctx, span] = otl::NewSpan("showTest", ctx);
        std::cout << "this is showtest\n";
    }

    void showTest2(const otl::Context &ctx) {
        auto [newctx, span] = otl::NewSpan("showTest2", ctx);
        std::cout << "this is showtest2\n";
    }

    ::grpc::Status GetName(::grpc::ServerContext *ctx, const helloworld::FamilyRequest *req, helloworld::FamilyResponse *rsp) {
        std::cout << "getname" << "\n";
        auto otlCtx = otl::ContextFromGrpcServerContext(ctx);
        showTest(otlCtx);
        showTest2(otlCtx);
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

void initOpentelemetry(int argc, char **argv) {
    otl::ExporterContainer c;
    c.push_back(otl::CreateConsoleExporter());
    c.push_back(otl::CreateHttpExporter("http://192.168.102.41:4318/v1/traces"));
    otl::Init(argc, argv, c);
}

void initServer() {
    cogrpc::DefSrvBuilder()->AddListeningPort("0.0.0.0:8080");
    cogrpc::DefSrvBuilder()->RegisterService<GreeterServer>();
    // 注册otl拦截器
    cogrpc::DefSrvBuilder()->AddInterceptor<otl::DefaultGrpcServerInterceptor>();
    cogrpc::DefSrvBuilder()->Run();
}

void initClient() {
    go [] {
        auto [ctx, span] = otl::NewSpan("initClient");
        auto grpcCtx = cogrpc::MakeContext(1000*10);
        grpcCtx = otl::MakeGrpcClientContext(grpcCtx, ctx);

        helloworld::HelloRequest req;
        helloworld::HelloReply rsp;
        req.set_name("this is initClient");

        auto cli = getClient();
        auto status = cli.SayHello(grpcCtx, req, &rsp);

        if (GRPC_OK(status)) {
            std::cout << "SayHello ok:" << rsp.ShortDebugString() << "\n";
        } else {
            std::cout << "SayHello error:" << GRPC_MSG(status) << "\n";
        }
    };
}

int main(int argc, char **argv) {
    initOpentelemetry(argc, argv);
    initServer();
    initClient();

    msleep(1000*10);
    return 0;
}