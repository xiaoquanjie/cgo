// 
// 此文件由工具自动生成的，请务修改 
// Tools built from xiaoqj 
// 

#pragma once 

#include "helloworld.grpc.pb.h" 
#include "cogrpc/cogrpc.h" 

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

class GreeterAgainServer : public cogrpc::Server<helloworld::GreeterAgain> {
public: 
    void InitMethod() override { 
         GRPC_SRV_UNARY_METHOD(SayHello, helloworld::HelloRequest, helloworld::HelloReply);
         GRPC_SRV_UNARY_METHOD(GetName, helloworld::FamilyRequest, helloworld::FamilyResponse);
         GRPC_SRV_BS_METHOD(ListName, helloworld::FamilyRequest, helloworld::FamilyResponse);
         GRPC_SRV_CS_METHOD(ClientStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);
         GRPC_SRV_SS_METHOD(ServerStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);
    }
    ::grpc::Status SayHello(::grpc::ServerContext *ctx, const helloworld::HelloRequest *req, helloworld::HelloReply *rsp) {
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

class NameServerServer : public cogrpc::Server<helloworld::NameServer> {
public: 
    void InitMethod() override { 
         GRPC_SRV_BS_METHOD(ListName, helloworld::FamilyRequest, helloworld::FamilyResponse);
    }
    ::grpc::Status ListName(::grpc::ServerContext *ctx, GRPC_SRV_RW(helloworld::FamilyRequest, helloworld::FamilyResponse) *rw) {
        return ::grpc::Status::OK;
    }

};

