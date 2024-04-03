// 
// 此文件由工具自动生成的，请务修改 
// Tools built from xiaoqj 
// 

#pragma once 

#include "helloworld.grpc.pb.h" 
#include "cogrpc/cogrpc.h" 

class GreeterClient : public cogrpc::Client<helloworld::Greeter> {
public: 
     GRPC_CLIENT_UNARY_METHOD(SayHello, helloworld::HelloRequest, helloworld::HelloReply);

     GRPC_CLIENT_UNARY_METHOD(GetName, helloworld::FamilyRequest, helloworld::FamilyResponse);

     GRPC_CLIENT_BS_METHOD(ListName, helloworld::FamilyRequest, helloworld::FamilyResponse);

     GRPC_CLIENT_CS_METHOD(ClientStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);

     GRPC_CLIENT_SS_METHOD(ServerStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);

};

class GreeterAgainClient : public cogrpc::Client<helloworld::GreeterAgain> {
public: 
     GRPC_CLIENT_UNARY_METHOD(SayHello, helloworld::HelloRequest, helloworld::HelloReply);

     GRPC_CLIENT_UNARY_METHOD(GetName, helloworld::FamilyRequest, helloworld::FamilyResponse);

     GRPC_CLIENT_BS_METHOD(ListName, helloworld::FamilyRequest, helloworld::FamilyResponse);

     GRPC_CLIENT_CS_METHOD(ClientStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);

     GRPC_CLIENT_SS_METHOD(ServerStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);

};

class NameServerClient : public cogrpc::Client<helloworld::NameServer> {
public: 
     GRPC_CLIENT_BS_METHOD(ListName, helloworld::FamilyRequest, helloworld::FamilyResponse);

};

