// 
// 此文件由工具自动生成的，请务修改 
// Tools built from xiaoqj 
// 

#pragma once 

#include "service.grpc.pb.h" 
#include "cogrpc/cogrpc.h" 

class TestServiceClient : public cogrpc::Client<protocol::TestService> {
public: 
     GRPC_CLIENT_UNARY_METHOD(GetUser, protocol::GetUserReq, protocol::GetUserRsp);

     GRPC_CLIENT_CS_METHOD(CStream, protocol::GetUserReq, protocol::GetUserRsp);

     GRPC_CLIENT_SS_METHOD(SStream, protocol::GetUserReq, protocol::GetUserRsp);

     GRPC_CLIENT_BS_METHOD(BStream, protocol::GetUserReq, protocol::GetUserRsp);

     GRPC_CLIENT_UNARY_METHOD(GetUser2, protocol::GetUserReq, protocol::GetUserRsp);

};

class HallServiceClient : public cogrpc::Client<protocol::HallService> {
public: 
     GRPC_CLIENT_UNARY_METHOD(GetUser, protocol::GetUserReq, protocol::GetUserRsp);

     GRPC_CLIENT_CS_METHOD(CStream, protocol::GetUserReq, protocol::GetUserRsp);

     GRPC_CLIENT_SS_METHOD(SStream, protocol::GetUserReq, protocol::GetUserRsp);

     GRPC_CLIENT_BS_METHOD(BStream, protocol::GetUserReq, protocol::GetUserRsp);

     GRPC_CLIENT_UNARY_METHOD(GetUser2, protocol::GetUserReq, protocol::GetUserRsp);

};

