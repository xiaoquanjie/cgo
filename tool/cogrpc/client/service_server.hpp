// 
// 此文件由工具自动生成的，请务修改 
// Tools built from xiaoqj 
// 

#pragma once 

#include "service.grpc.pb.h" 
#include "cogrpc/cogrpc.h" 

class TestServiceServer : public cogrpc::Server<protocol::TestService> {
public: 
    void InitMethod() override { 
         GRPC_SRV_UNARY_METHOD(GetUser, protocol::GetUserReq, protocol::GetUserRsp);
         GRPC_SRV_CS_METHOD(CStream, protocol::GetUserReq, protocol::GetUserRsp);
         GRPC_SRV_SS_METHOD(SStream, protocol::GetUserReq, protocol::GetUserRsp);
         GRPC_SRV_BS_METHOD(BStream, protocol::GetUserReq, protocol::GetUserRsp);
         GRPC_SRV_UNARY_METHOD(GetUser2, protocol::GetUserReq, protocol::GetUserRsp);
    }
    ::grpc::Status GetUser(::grpc::ServerContext *ctx, const protocol::GetUserReq *req, protocol::GetUserRsp *rsp) {
        return ::grpc::Status::OK;
    }

    ::grpc::Status CStream(::grpc::ServerContext *ctx, GRPC_SRV_READER(protocol::GetUserReq, protocol::GetUserRsp) *reader, protocol::GetUserRsp *rsp) {
        return ::grpc::Status::OK;
    }

    ::grpc::Status SStream(::grpc::ServerContext *ctx, const protocol::GetUserReq *req, GRPC_SRV_WRITER(protocol::GetUserReq, protocol::GetUserRsp) *writer) {
        return ::grpc::Status::OK;
    }

    ::grpc::Status BStream(::grpc::ServerContext *ctx, GRPC_SRV_RW(protocol::GetUserReq, protocol::GetUserRsp) *rw) {
    }

    ::grpc::Status GetUser2(::grpc::ServerContext *ctx, const protocol::GetUserReq *req, protocol::GetUserRsp *rsp) {
        return ::grpc::Status::OK;
    }

};

class HallServiceServer : public cogrpc::Server<protocol::HallService> {
public: 
    void InitMethod() override { 
         GRPC_SRV_UNARY_METHOD(GetUser, protocol::GetUserReq, protocol::GetUserRsp);
         GRPC_SRV_CS_METHOD(CStream, protocol::GetUserReq, protocol::GetUserRsp);
         GRPC_SRV_SS_METHOD(SStream, protocol::GetUserReq, protocol::GetUserRsp);
         GRPC_SRV_BS_METHOD(BStream, protocol::GetUserReq, protocol::GetUserRsp);
         GRPC_SRV_UNARY_METHOD(GetUser2, protocol::GetUserReq, protocol::GetUserRsp);
    }
    ::grpc::Status GetUser(::grpc::ServerContext *ctx, const protocol::GetUserReq *req, protocol::GetUserRsp *rsp) {
        return ::grpc::Status::OK;
    }

    ::grpc::Status CStream(::grpc::ServerContext *ctx, GRPC_SRV_READER(protocol::GetUserReq, protocol::GetUserRsp) *reader, protocol::GetUserRsp *rsp) {
        return ::grpc::Status::OK;
    }

    ::grpc::Status SStream(::grpc::ServerContext *ctx, const protocol::GetUserReq *req, GRPC_SRV_WRITER(protocol::GetUserReq, protocol::GetUserRsp) *writer) {
        return ::grpc::Status::OK;
    }

    ::grpc::Status BStream(::grpc::ServerContext *ctx, GRPC_SRV_RW(protocol::GetUserReq, protocol::GetUserRsp) *rw) {
    }

    ::grpc::Status GetUser2(::grpc::ServerContext *ctx, const protocol::GetUserReq *req, protocol::GetUserRsp *rsp) {
        return ::grpc::Status::OK;
    }

};

