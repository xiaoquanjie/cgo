//
// Created by xiaoqj on 2023/7/10.
//

#pragma once

#include "rmdbclient/rmdb/rmdb.grpc.pb.h"
#include "co_grpc/macro.h"

namespace rmdbclient {

// 协程接口
class RmdbClientImplement : public co_grpc::Client<rmdb::RmDb> {
public:
    GRPC_CLIENT_CO_UNARY_METHOD(Set, rmdb::SetDataReq, rmdb::SetDataRsp);

    GRPC_CLIENT_CO_UNARY_METHOD(SetNx, rmdb::SetNxDataReq, rmdb::SetNxDataRsp);

    GRPC_CLIENT_CO_UNARY_METHOD(Get, rmdb::GetDataReq, rmdb::GetDataRsp);

    GRPC_CLIENT_CO_UNARY_METHOD(Del, rmdb::DelDataReq, rmdb::DelDataRsp);

    GRPC_CLIENT_CO_UNARY_METHOD(Update, rmdb::UpdateDataReq, rmdb::UpdateDataRsp);

    GRPC_CLIENT_CO_UNARY_METHOD(BatchSet, rmdb::BatchSetDataReq, rmdb::BatchSetDataRsp);

    GRPC_CLIENT_CO_UNARY_METHOD(BatchSetNx, rmdb::BatchSetNxDataReq, rmdb::BatchSetNxDataRsp);

    GRPC_CLIENT_CO_UNARY_METHOD(BatchGet, rmdb::BatchGetDataReq, rmdb::BatchGetDataRsp);

    GRPC_CLIENT_CO_UNARY_METHOD(BatchDel, rmdb::BatchDelDataReq, rmdb::BatchDelDataRsp);

    GRPC_CLIENT_CO_UNARY_METHOD(BatchUpdate, rmdb::BatchUpdateDataReq, rmdb::BatchUpdateDataRsp);

    GRPC_CLIENT_CO_UNARY_METHOD(GetCount, rmdb::GetDataCountReq, rmdb::GetDataCountRsp);

    GRPC_CLIENT_CO_UNARY_METHOD(GetMeta, rmdb::GetMetaReq, rmdb::GetMetaRsp);

    GRPC_CLIENT_CO_UNARY_METHOD(ClusterInfo, rmdb::ClusterInfoReq, rmdb::ClusterInfoRsp);

    GRPC_CLIENT_CO_UNARY_METHOD(Match, rmdb::MatchDataReq, rmdb::MatchDataRsp);

    GRPC_CLIENT_CO_UNARY_METHOD(Find, rmdb::FindDataReq, rmdb::FindDataRsp);

    GRPC_CLIENT_CO_UNARY_METHOD(UpdateKey, rmdb::UpdateKeyReq, rmdb::UpdateKeyRsp);

};

}