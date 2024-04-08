//
// Created by xiaoqj on 2024/4/2.
//

#include <cgo/thirdparty/cogrpc/cogrpc.h>
#include "common/helloworld.grpc.pb.h"

class GreeterClient : public cogrpc::Client<helloworld::Greeter> {
public:
    GRPC_CLIENT_UNARY_METHOD(SayHello, helloworld::HelloRequest, helloworld::HelloReply);

    GRPC_CLIENT_UNARY_METHOD(GetName, helloworld::FamilyRequest, helloworld::FamilyResponse);

    GRPC_CLIENT_BS_METHOD(ListName, helloworld::FamilyRequest, helloworld::FamilyResponse);

    GRPC_CLIENT_CS_METHOD(ClientStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);

    GRPC_CLIENT_SS_METHOD(ServerStreamSayHello, helloworld::HelloRequest, helloworld::HelloReply);

};

void unary_test(GreeterClient& client) {
    helloworld::HelloRequest req;
    req.set_name("myname");
    helloworld::HelloReply rsp;
    auto s = client.SayHello(nullptr, req, &rsp);
    if (GRPC_OK(s)) {
        std::cout << "unary SayHello: " << rsp.ShortDebugString() << "\n";
    } else {
        std::cout << GRPC_MSG(s) << "\n";
    }
}

void double_test(GreeterClient& client) {
    helloworld::FamilyRequest req;
    req.set_family("myfamily");
    helloworld::FamilyResponse rsp;
    auto rw = client.ListName(nullptr);
    if (rw) {
        rw->Write(req);
        if (rw->Read(&rsp)) {
            std::cout << "double stream ListName: " << rsp.ShortDebugString() << "\n";
        } else {
            std::cout << "ListName stream read error\n";
        }
    } else {
        std::cout << "get ListName stream error\n";
    }
}

void client_test(GreeterClient& client) {
    helloworld::HelloRequest req;
    helloworld::HelloReply rsp;
    req.set_name("ClientStreamSayHello");
    auto w = client.ClientStreamSayHello(nullptr, &rsp);
    return;
    if (w) {
        if (w->Write(req)) {
            w->Finish();
            std::cout << "client stream ClientStreamSayHello: " << rsp.ShortDebugString() << "\n";
        } else {
            std::cout << "ClientStreamSayHello stream write error\n";
        }
    } else {
        std::cout << "get ClientStreamSayHello stream error\n";
    }

    std::cout << "use count:" << w.use_count() << "\n";
}

void serve_test(GreeterClient& client) {
    helloworld::HelloRequest req;
    req.set_name("ServerStreamSayHello");
    helloworld::HelloReply rsp;
    auto r = client.ServerStreamSayHello(nullptr, req);
    if (r) {
        if (r->Read(&rsp)) {
            std::cout << "server stream ServerStreamSayHello: " << rsp.ShortDebugString() << "\n";
        } else {
            std::cout << "ServerStreamSayHello stream read error\n";
        }
    } else {
        std::cout << "get ServerStreamSayHello stream error\n";
    }
}

int main() {
    cgo::WaitGroup wg;
    wg.Add(1);

    go [&wg]
    {
        GreeterClient client;
        client.Bind("127.0.0.1:10052", "");

        //unary_test(client);
        client_test(client);
        //serve_test(client);


        std::cout << "over\n";
        wg.Done();
    };

    wg.Wait();
    return 0;
}