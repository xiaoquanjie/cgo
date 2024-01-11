// 
// 此文件由工具自动生成的，请务修改 
// Tools built from xiaoqj 
// 

#pragma once 

#include "protocol/service.grpc.pb.h" 
#include "co_grpc/client/client.h" 

namespace grpcm {

    class TestServiceClient : public co_grpc::Client<protocol::TestService> {
    public: 

        TestServiceClient {} 

GRPC_CLIENT_CO_UNARY_METHOD(GetUser, GetUserReq, GetUserRsp);

        }

GRPC_CLIENT_CO_CS_METHOD(CStream, GetUserReq, GetUserRsp);

        }

GRPC_CLIENT_CO_SS_METHOD(SStream, GetUserReq, GetUserRsp);

        }

GRPC_CLIENT_CO_BS_METHOD(BStream, GetUserReq, GetUserRsp);

        }

GRPC_CLIENT_CO_UNARY_METHOD(GetUser2, GetUserReq, GetUserRsp);

        }

    };

    class HallServiceClient : public co_grpc::Client<protocol::HallService> {
    public: 

        HallServiceClient {} 

GRPC_CLIENT_CO_UNARY_METHOD(GetUser, GetUserReq, GetUserRsp);

        }

GRPC_CLIENT_CO_CS_METHOD(CStream, GetUserReq, GetUserRsp);

        }

GRPC_CLIENT_CO_SS_METHOD(SStream, GetUserReq, GetUserRsp);

        }

GRPC_CLIENT_CO_BS_METHOD(BStream, GetUserReq, GetUserRsp);

        }

GRPC_CLIENT_CO_UNARY_METHOD(GetUser2, GetUserReq, GetUserRsp);

        }

    };

}
