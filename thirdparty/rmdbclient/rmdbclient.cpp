//
// Created by xiaoqj on 2023/7/10.
//

#include "rmdbclient/rmdbclient.h"
#include "rmdbclient/implement.h"
#include "co_grpc/client/client_builder.h"
#include "rmdbclient/encode.h"
#include "rmdbclient/algorithm.h"
#include "co_grpc/macro.h"
#include "rmdbclient/rmdb/config.pb.h"
#include <chrono>
#include <vector>
#include <bson.h>
#include <fstream>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

// 获取metadata
#undef M_RESET_MD
#define M_RESET_MD() \
rmdb::MetaData& md = *request.mutable_meta();     \
if (md.db().empty()) md.set_db(this->dbname_);

// 获取客户端实现
#undef M_GET_IMPL_CLIENT
#define M_GET_IMPL_CLIENT() \
RmdbClientImplement cli;        \
auto ctx = newContext(timeout);   \
cli.Bind(this->getHost(&md), ""); \
if (!cli.Valid()) { return (int) ::grpc::INTERNAL; }

#undef M_GET_IMPL_CLIENT_BYSLOT
#define M_GET_IMPL_CLIENT_BYSLOT(slot) \
RmdbClientImplement cli;        \
auto ctx = newContext(timeout);   \
cli.Bind(this->getHost(slot), ""); \


namespace rmdbclient {

    struct GroupUnit {
        const rmdb::MetaData *meta = nullptr;
        int idx = 0;
        const char *data = nullptr;
        size_t dataLen = 0;
    };

    // 分组
    void groupingData(const std::string &db,
                      uint32_t slots,
                      google::protobuf::RepeatedPtrField<rmdb::MetaAndData> &metaAndData,
                      std::unordered_map<int, std::vector<GroupUnit>> &group) {
        for (int idx = 0; idx < metaAndData.size(); idx++) {
            auto &md = *metaAndData[idx].mutable_meta();
            if (md.db().empty()) {
                md.set_db(db);
            }

            std::string key;
            meta2Key(&md, key);
            auto slot = CalcSlot(key, slots);

            GroupUnit unit;
            unit.meta = &md;
            unit.idx = idx;
            unit.data = metaAndData.Get(idx).data().c_str();
            unit.dataLen = metaAndData.Get(idx).data().size();

            group[slot].push_back(unit);
        }
    }

    void groupingData(const std::string &db,
                      uint32_t slots,
                      google::protobuf::RepeatedPtrField<rmdb::MetaData> &metas,
                      std::unordered_map<int, std::vector<GroupUnit>> &group) {
        for (int idx = 0; idx < metas.size(); idx++) {
            auto& md = metas[idx];
            if (md.db().empty()) {
                md.set_db(db);
            }

            std::string key;
            meta2Key(&md, key);
            auto slot = CalcSlot(key, slots);

            GroupUnit unit;
            unit.meta = &md;
            unit.idx = idx;
            group[slot].push_back(unit);
        }
    }

    std::shared_ptr<::grpc::ClientContext> newContext(uint32_t timeout) {
        auto ctx = std::make_shared<::grpc::ClientContext>();
        if (timeout != 0) {
            auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(timeout);
            ctx->set_deadline(deadline);
        }
        return ctx;
    }

    RmdbClient::RmdbClient() {
        impl_ = 0;
    }

    RmdbClient::~RmdbClient() {
        if (impl_) {
            delete impl_;
        }
    }

    bool RmdbClient::Dial(const std::string &target) {
        // 已经拨打过了
        if (impl_) {
            return false;
        }

        std::vector<std::string> arr;
        split(target, "/", arr);

        std::string addr = target;
        if (arr.size() == 2) {
            this->dbname_ = arr[1];
            addr = arr[0];
        }

        impl_ = new RmdbClientImplement;
        impl_->Bind(addr, "");
        return true;
    }

    bool RmdbClient::updateCluster() {
        if (!this->impl_) {
            return false;
        }

        time_t now = 0;
        time(&now);

        if (this->cluster_.online && now - this->cluster_.lastUpdate <= 2) {
            return true;
        }

        rmdb::ClusterInfoReq request;
        rmdb::ClusterInfoRsp response;
        auto ctx = newContext(3000);

        auto status = this->impl_->ClusterInfo(ctx, request, &response);
        if (!GRPC_OK(status)) {
            return false;
        }

        std::unique_lock<std::shared_mutex> sl(this->mu_);

        this->cluster_.lastUpdate = now;
        this->cluster_.online = response.online();
        this->cluster_.slots = response.slots();
        this->cluster_.target.clear();
        for (auto kv: response.targets()) {
            this->cluster_.target[kv.first] = kv.second;
        }

        return true;
    }

    uint32_t RmdbClient::getSlots() {
        auto get = [this]() -> uint32_t {
            std::shared_lock<std::shared_mutex> sl(this->mu_);
            if (this->cluster_.online && this->cluster_.slots != 0) {
                return this->cluster_.slots;
            }
            return 0;
        };

        auto slots = get();
        if (slots == 0) {
            // 更新集群
            if (this->updateCluster()) {
                slots = get();
            }
        }

        return slots;
    }

    const std::string& RmdbClient::getHost(uint32_t slot) {
        auto get = [this](uint32_t slot) -> const std::string& {
            std::shared_lock<std::shared_mutex> sl(this->mu_);
            if (this->cluster_.online && this->cluster_.slots != 0) {
                auto iter = this->cluster_.target.find(slot);
                if (iter != this->cluster_.target.end()) {
                    return iter->second;
                }
            }
            static std::string empty;
            return empty;
        };

        auto& host = get(slot);
        if (host.empty()) {
            // 更新集群
            if (this->updateCluster()) {
                return get(slot);
            }
        }

        return host;
    }

    const std::string& RmdbClient::getHost(const rmdb::MetaData *meta) {
        auto get = [this](const std::string& key) -> const std::string& {
            std::shared_lock<std::shared_mutex> sl(this->mu_);
            if (this->cluster_.online && this->cluster_.slots != 0) {
                uint32_t slot = CalcSlot(key, this->cluster_.slots);
                auto iter = this->cluster_.target.find(slot);
                if (iter != this->cluster_.target.end()) {
                    return iter->second;
                }
            }
            static std::string empty;
            return empty;
        };

        std::string key;
        meta2Key(meta, key);

        auto& host = get(key);
        if (host.empty()) {
            // 更新集群
            if (this->updateCluster()) {
                return get(key);
            }
        }

        return host;
    }

    int RmdbClient::Set(rmdb::SetDataReq &request, rmdb::SetDataRsp *response, uint32_t timeout) {
        ::grpc::Status status = ::grpc::Status::OK;
        M_RESET_MD();

        for (int retry = 1; retry <= 2; retry++) {
            M_GET_IMPL_CLIENT();
            status = cli.Set(ctx, request, response);
            if (!GRPC_OK(status)) {
                return status.error_code();
            }

            if (response->status().code() == rmdb::ErrorCode::ERR_MOVE) {
                this->updateCluster();
                continue;
            }
            break;
        }

        return status.error_code();
    }

    int RmdbClient::set(const std::string &table,
                        const std::unordered_map<std::string, std::string> &keys,
                        const char *data,
                        size_t dataLen,
                        rmdb::ContentType contentType,
                        rmdb::SetDataRsp *response,
                        uint32_t timeout) {
        rmdb::SetDataReq request;
        request.mutable_meta()->set_table(table);
        request.mutable_meta()->set_type(contentType);
        for (auto &kv: keys) {
            auto &mp = *request.mutable_meta()->mutable_keys();
            mp[kv.first] = kv.second;
        }

        request.set_data(data, dataLen);
        return this->Set(request, response, timeout);
    }

    std::pair<int, rmdb::StatusData> RmdbClient::SetPb(const std::string &table,
                                                       const std::unordered_map<std::string, std::string> &keys,
                                                       const ::google::protobuf::Message &msg,
                                                       uint32_t timeout) {
        std::string data;
        Proto2BsonBytes(&msg, data);

        rmdb::SetDataRsp response;
        auto status = this->set(table, keys, data.c_str(), data.size(), rmdb::ContentType::BSON, &response, timeout);
        return std::make_pair(status, response.status());
    }

    std::pair<int, rmdb::StatusData> RmdbClient::SetKv(const std::string &table,
                                                       const std::unordered_map<std::string, std::string> &keys,
                                                       const std::unordered_map<std::string, std::string> &value,
                                                       uint32_t timeout) {
        void *b = Kv2Bson(value);
        int len = 0;
        auto data = bsonBytes(b, &len);

        rmdb::SetDataRsp response;
        auto status = this->set(table, keys, data, len, rmdb::ContentType::BSON, &response, timeout);

        freeBson(b);
        return std::make_pair(status, response.status());
    }

    std::pair<int, rmdb::StatusData> RmdbClient::SetPbBinary(const std::string &table,
                                                             const std::unordered_map<std::string, std::string> &keys,
                                                             const ::google::protobuf::Message &msg,
                                                             uint32_t timeout) {
        std::string data;
        msg.SerializeToString(&data);

        rmdb::SetDataRsp response;
        auto status = this->set(table, keys, data.c_str(), data.size(), rmdb::ContentType::binary, &response, timeout);
        return std::make_pair(status, response.status());
    }

    std::pair<int, rmdb::StatusData> RmdbClient::SetString(const std::string &table,
                                                           const std::unordered_map<std::string, std::string> &keys,
                                                           const std::string &data,
                                                           uint32_t timeout) {
        rmdb::SetDataRsp response;
        auto status = this->set(table, keys, data.c_str(), data.size(), rmdb::ContentType::PLAIN, &response, timeout);
        std::pair<int, rmdb::StatusData> p;
        p.first = status;
        p.second = response.status();
        return p;
    }

    std::pair<int, rmdb::StatusData> RmdbClient::SetBinary(const std::string &table,
                                                           const std::unordered_map<std::string, std::string> &keys,
                                                           const char *data,
                                                           size_t dataLen,
                                                           uint32_t timeout) {
        rmdb::SetDataRsp response;
        auto status = this->set(table, keys, data, dataLen, rmdb::ContentType::binary, &response, timeout);
        std::pair<int, rmdb::StatusData> p;
        p.first = status;
        p.second = response.status();
        return p;
    }

    int RmdbClient::SetNx(rmdb::SetNxDataReq &request, rmdb::SetNxDataRsp *response, uint32_t timeout) {
        ::grpc::Status status = ::grpc::Status::OK;
        M_RESET_MD();

        for (int retry = 1; retry <= 2; retry++) {
            M_GET_IMPL_CLIENT();
            status = cli.SetNx(ctx, request, response);
            if (!GRPC_OK(status)) {
                return status.error_code();
            }

            if (response->status().code() == rmdb::ErrorCode::ERR_MOVE) {
                this->updateCluster();
                continue;
            }
            break;
        }

        return status.error_code();
    }

    int RmdbClient::setNx(const std::string &table,
                          const std::unordered_map<std::string, std::string> &keys,
                          const char *data,
                          size_t dataLen,
                          rmdb::ContentType contentType,
                          rmdb::SetNxDataRsp *response,
                          uint32_t timeout) {
        rmdb::SetNxDataReq request;
        request.mutable_meta()->set_table(table);
        request.mutable_meta()->set_type(contentType);
        for (auto &kv: keys) {
            auto &mp = *request.mutable_meta()->mutable_keys();
            mp[kv.first] = kv.second;
        }

        request.set_data(data, dataLen);
        return this->SetNx(request, response, timeout);
    }

    std::pair<int, rmdb::StatusData> RmdbClient::SetNxPb(const std::string &table,
                                                         const std::unordered_map<std::string, std::string> &keys,
                                                         const ::google::protobuf::Message &msg,
                                                         uint32_t timeout) {
        std::string data;
        Proto2BsonBytes(&msg, data);

        rmdb::SetNxDataRsp response;
        auto status = this->setNx(table, keys, data.c_str(), data.size(), rmdb::ContentType::BSON, &response, timeout);
        return std::make_pair(status, response.status());
    }

    std::pair<int, rmdb::StatusData> RmdbClient::SetNxKv(const std::string &table,
                                                         const std::unordered_map<std::string, std::string> &keys,
                                                         const std::unordered_map<std::string, std::string> &value,
                                                         uint32_t timeout) {
        void *b = Kv2Bson(value);
        int len = 0;
        auto data = bsonBytes(b, &len);

        rmdb::SetNxDataRsp response;
        auto status = this->setNx(table, keys, data, len, rmdb::ContentType::BSON, &response, timeout);

        freeBson(b);
        return std::make_pair(status, response.status());
    }

    std::pair<int, rmdb::StatusData> RmdbClient::SetNxPbBinary(const std::string &table,
                                                               const std::unordered_map<std::string, std::string> &keys,
                                                               const ::google::protobuf::Message &msg,
                                                               uint32_t timeout) {
        std::string data;
        msg.SerializeToString(&data);

        rmdb::SetNxDataRsp response;
        auto status = this->setNx(table, keys, data.c_str(), data.size(), rmdb::ContentType::binary, &response,
                                  timeout);
        return std::make_pair(status, response.status());
    }

    std::pair<int, rmdb::StatusData> RmdbClient::SetNxString(const std::string &table,
                                                             const std::unordered_map<std::string, std::string> &keys,
                                                             const std::string &data,
                                                             uint32_t timeout) {
        rmdb::SetNxDataRsp response;
        auto status = this->setNx(table, keys, data.c_str(), data.size(), rmdb::ContentType::PLAIN, &response, timeout);
        std::pair<int, rmdb::StatusData> p;
        p.first = status;
        p.second = response.status();
        return p;
    }

    std::pair<int, rmdb::StatusData> RmdbClient::SetNxBinary(const std::string &table,
                                                             const std::unordered_map<std::string, std::string> &keys,
                                                             const char *data,
                                                             size_t dataLen,
                                                             uint32_t timeout) {
        rmdb::SetNxDataRsp response;
        auto status = this->setNx(table, keys, data, dataLen, rmdb::ContentType::binary, &response, timeout);
        std::pair<int, rmdb::StatusData> p;
        p.first = status;
        p.second = response.status();
        return p;
    }

    int RmdbClient::Get(rmdb::GetDataReq &request, rmdb::GetDataRsp *response, uint32_t timeout) {
        ::grpc::Status status = ::grpc::Status::OK;
        M_RESET_MD();

        for (int retry = 1; retry <= 2; retry++) {
            M_GET_IMPL_CLIENT();
            status = cli.Get(ctx, request, response);
            if (!GRPC_OK(status)) {
                return status.error_code();
            }

            if (response->status().code() == rmdb::ErrorCode::ERR_MOVE) {
                this->updateCluster();
                continue;
            }
            break;
        }

        return status.error_code();
    }

    std::pair<int, rmdb::StatusData> RmdbClient::GetPb(const std::string &table,
                                                       const std::unordered_map<std::string, std::string> &keys,
                                                       ::google::protobuf::Message *msg,
                                                       uint32_t timeout) {
        rmdb::GetDataReq request;
        rmdb::GetDataRsp response;

        request.mutable_meta()->set_table(table);
        for (auto &kv: keys) {
            auto &mp = *request.mutable_meta()->mutable_keys();
            mp[kv.first] = kv.second;
        }

        std::pair<int, rmdb::StatusData> p;
        p.first = this->Get(request, &response, timeout);
        if (p.first == 0 && response.status().code() == rmdb::ErrorCode::OK) {
            BsonBytes2Proto(response.data(), msg);
        }

        p.second = response.status();
        return p;
    }

    std::pair<int, rmdb::StatusData> RmdbClient::GetKv(const std::string &table,
                                                       const std::unordered_map<std::string, std::string> &keys,
                                                       std::unordered_map<std::string, std::string> &value,
                                                       uint32_t timeout) {
        std::string data;
        auto ret = GetBinary(table, keys, data, timeout);
        if (ret.first == grpc::StatusCode::OK && ret.second.code() == rmdb::OK) {
            BsonBytes2Kv(data, value);
        }
        return ret;
    }

    std::pair<int, rmdb::StatusData> RmdbClient::GetPbBinary(const std::string &table,
                                                             const std::unordered_map<std::string, std::string> &keys,
                                                             ::google::protobuf::Message *msg,
                                                             uint32_t timeout) {
        std::string data;
        auto ret = GetBinary(table, keys, data, timeout);
        if (ret.first == grpc::StatusCode::OK && ret.second.code() == rmdb::OK) {
            msg->ParseFromString(data);
        }
        return ret;
    }

    std::pair<int, rmdb::StatusData> RmdbClient::GetBinary(const std::string &table,
                                                           const std::unordered_map<std::string, std::string> &keys,
                                                           std::string &data,
                                                           uint32_t timeout) {
        rmdb::GetDataReq request;
        rmdb::GetDataRsp response;

        request.mutable_meta()->set_table(table);
        for (auto &kv: keys) {
            auto &mp = *request.mutable_meta()->mutable_keys();
            mp[kv.first] = kv.second;
        }

        std::pair<int, rmdb::StatusData> p;
        p.first = this->Get(request, &response, timeout);
        if (p.first == 0 && response.status().code() == rmdb::ErrorCode::OK) {
            response.mutable_data()->swap(data);
        }

        p.second = response.status();
        return p;
    }

    std::pair<int, rmdb::StatusData> RmdbClient::GetString(const std::string &table,
                                                           const std::unordered_map<std::string, std::string> &keys,
                                                           std::string &data,
                                                           uint32_t timeout) {
        return GetBinary(table, keys, data, timeout);
    }

    int RmdbClient::Del(rmdb::DelDataReq &request, rmdb::DelDataRsp *response, uint32_t timeout) {
        auto status = grpc::Status::OK;
        M_RESET_MD();

        for (int retry = 1; retry <= 2; retry++) {
            M_GET_IMPL_CLIENT();
            status = cli.Del(ctx, request, response);
            if (!GRPC_OK(status)) {
                return status.error_code();
            }

            if (response->status().code() == rmdb::ErrorCode::ERR_MOVE) {
                this->updateCluster();
                continue;
            }
            break;
        }

        return status.error_code();
    }

    std::pair<int, rmdb::StatusData> RmdbClient::DelData(const std::string &table,
                                                         const std::unordered_map<std::string, std::string> &keys,
                                                         uint32_t timeout) {
        rmdb::DelDataReq request;
        rmdb::DelDataRsp response;

        request.mutable_meta()->set_table(table);
        for (auto &kv: keys) {
            auto &mp = *request.mutable_meta()->mutable_keys();
            mp[kv.first] = kv.second;
        }

        std::pair<int, rmdb::StatusData> p;
        p.first = this->Del(request, &response, timeout);
        p.second = response.status();
        return p;
    }

    int RmdbClient::Update(rmdb::UpdateDataReq &request, rmdb::UpdateDataRsp *response, uint32_t timeout) {
        ::grpc::Status status = ::grpc::Status::OK;
        M_RESET_MD();

        for (int retry = 1; retry <= 2; retry++) {
            M_GET_IMPL_CLIENT();
            status = cli.Update(ctx, request, response);
            if (!GRPC_OK(status)) {
                return status.error_code();
            }

            if (response->status().code() == rmdb::ErrorCode::ERR_MOVE) {
                this->updateCluster();
                continue;
            }
            break;
        }

        return status.error_code();
    }

    int RmdbClient::update(const std::string &table,
                           const std::unordered_map<std::string, std::string> &keys,
                           const char *data,
                           size_t dataLen,
                           rmdb::ContentType contentType,
                           rmdb::UpdateDataRsp *response,
                           uint32_t timeout) {
        rmdb::UpdateDataReq request;
        request.mutable_meta()->set_table(table);
        request.mutable_meta()->set_type(contentType);
        for (auto &kv: keys) {
            auto &mp = *request.mutable_meta()->mutable_keys();
            mp[kv.first] = kv.second;
        }

        request.set_data(data, dataLen);
        return this->Update(request, response, timeout);
    }

    std::pair<int, rmdb::StatusData> RmdbClient::UpdatePb(const std::string &table,
                                                          const std::unordered_map<std::string, std::string> &keys,
                                                          const ::google::protobuf::Message &msg,
                                                          uint32_t timeout) {
        std::string data;
        Proto2BsonBytes(&msg, data);

        rmdb::UpdateDataRsp response;
        auto status = this->update(table, keys, data.c_str(), data.size(), rmdb::ContentType::BSON, &response, timeout);
        return std::make_pair(status, response.status());
    }

    std::pair<int, rmdb::StatusData> RmdbClient::UpdateKv(const std::string &table,
                                                          const std::unordered_map<std::string, std::string> &keys,
                                                          const std::unordered_map<std::string, std::string> &value,
                                                          uint32_t timeout) {
        void *b = Kv2Bson(value);
        int len = 0;
        auto data = bsonBytes(b, &len);

        rmdb::UpdateDataRsp response;
        auto status = this->update(table, keys, data, len, rmdb::ContentType::BSON, &response, timeout);

        freeBson(b);
        return std::make_pair(status, response.status());
    }

    std::pair<int, rmdb::StatusData> RmdbClient::UpdatePbBinary(const std::string &table,
                                                                const std::unordered_map<std::string, std::string> &keys,
                                                                const ::google::protobuf::Message &msg,
                                                                uint32_t timeout) {
        std::string data;
        msg.SerializeToString(&data);

        rmdb::UpdateDataRsp response;
        auto status = this->update(table, keys, data.c_str(), data.size(), rmdb::ContentType::binary, &response,
                                   timeout);
        return std::make_pair(status, response.status());
    }

    std::pair<int, rmdb::StatusData> RmdbClient::UpdateString(const std::string &table,
                                                              const std::unordered_map<std::string, std::string> &keys,
                                                              const std::string &data,
                                                              uint32_t timeout) {
        rmdb::UpdateDataRsp response;
        auto status = this->update(table, keys, data.c_str(), data.size(), rmdb::ContentType::PLAIN, &response,
                                   timeout);
        std::pair<int, rmdb::StatusData> p;
        p.first = status;
        p.second = response.status();
        return p;
    }

    std::pair<int, rmdb::StatusData> RmdbClient::UpdateBinary(const std::string &table,
                                                              const std::unordered_map<std::string, std::string> &keys,
                                                              const char *data,
                                                              size_t dataLen,
                                                              uint32_t timeout) {
        rmdb::UpdateDataRsp response;
        auto status = this->update(table, keys, data, dataLen, rmdb::ContentType::binary, &response, timeout);
        std::pair<int, rmdb::StatusData> p;
        p.first = status;
        p.second = response.status();
        return p;
    }

    int RmdbClient::BatchSet(rmdb::BatchSetDataReq &request, rmdb::BatchSetDataRsp *response, uint32_t timeout) {
        auto slots = this->getSlots();
        if (slots == 0) {
            return (int) ::grpc::INTERNAL;
        }

        std::unordered_map<int, std::vector<GroupUnit>> group;
        groupingData(this->dbname_, slots, *request.mutable_meta_and_data(), group);
        for (int i = 0; i < request.meta_and_data_size(); i++) {
            response->add_status();
        }

        bool errMove = false;
        auto fill = [&errMove](rmdb::BatchSetDataRsp *rsp, std::vector<GroupUnit> &units, rmdb::BatchSetDataRsp *out) {
            for (size_t i = 0; i < units.size(); i++) {
                auto g = units[i];
                auto idx = g.idx;

                auto status = rsp->mutable_status(idx);
                if (!out) {
                    status->set_code(rmdb::ErrorCode::CLIENT_NOT_CONNECTED);
                } else {
                    status->CopyFrom(out->status(i));
                    if (status->code() == rmdb::ErrorCode::ERR_MOVE) {
                        errMove = true;
                    }
                }
            }
        };

        for (auto &kv: group) {
            rmdb::BatchSetDataReq in;
            for (size_t i = 0; i < kv.second.size(); i++) {
                auto metaAndData = in.add_meta_and_data();
                metaAndData->mutable_meta()->CopyFrom(*kv.second[i].meta);
                metaAndData->set_data(kv.second[i].data, kv.second[i].dataLen);
            }

            M_GET_IMPL_CLIENT_BYSLOT(kv.first);
            if (!cli.Valid()) {
                fill(response, kv.second, nullptr);
                continue;
            }

            rmdb::BatchSetDataRsp out;
            auto status = cli.BatchSet(ctx, in, &out);
            if (!GRPC_OK(status)) {
                fill(response, kv.second, nullptr);
            } else {
                fill(response, kv.second, &out);
            }
        }

        if (errMove) {
            this->updateCluster();
        }
        return grpc::Status::OK.error_code();
    }

    int RmdbClient::BatchGet(rmdb::BatchGetDataReq &request, rmdb::BatchGetDataRsp *response, uint32_t timeout) {
        auto slots = this->getSlots();
        if (slots == 0) {
            return (int) ::grpc::INTERNAL;
        }

        std::unordered_map<int, std::vector<GroupUnit>> group;
        groupingData(this->dbname_, slots, *request.mutable_metas(), group);
        for (int i = 0; i < request.metas_size(); i++) {
            response->add_status_and_data();
        }

        bool errMove = false;
        auto fill = [&errMove](rmdb::BatchGetDataRsp *rsp, std::vector<GroupUnit> &units, rmdb::BatchGetDataRsp *out) {
            for (size_t i = 0; i < units.size(); i++) {
                auto g = units[i];
                auto idx = g.idx;

                auto md = rsp->mutable_status_and_data(idx);
                if (!out) {
                    md->mutable_status()->set_code(rmdb::ErrorCode::CLIENT_NOT_CONNECTED);
                } else {
                    md->CopyFrom(out->status_and_data(i));
                    if (md->status().code() == rmdb::ErrorCode::ERR_MOVE) {
                        errMove = true;
                    }
                }
            }
        };

        for (auto &kv: group) {
            rmdb::BatchGetDataReq in;
            for (size_t i = 0; i < kv.second.size(); i++) {
                auto meta = in.add_metas();
                meta->CopyFrom(*kv.second[i].meta);
            }

            M_GET_IMPL_CLIENT_BYSLOT(kv.first);
            if (!cli.Valid()) {
                fill(response, kv.second, nullptr);
                continue;
            }

            rmdb::BatchGetDataRsp out;
            auto status = cli.BatchGet(ctx, in, &out);
            if (!GRPC_OK(status)) {
                fill(response, kv.second, nullptr);
            } else {
                fill(response, kv.second, &out);
            }
        }

        if (errMove) {
            this->updateCluster();
        }
        return grpc::Status::OK.error_code();
    }

    int RmdbClient::BatchDel(rmdb::BatchDelDataReq &request, rmdb::BatchDelDataRsp *response, uint32_t timeout) {
        auto slots = this->getSlots();
        if (slots == 0) {
            return (int) ::grpc::INTERNAL;
        }

        std::unordered_map<int, std::vector<GroupUnit>> group;
        groupingData(this->dbname_, slots, *request.mutable_metas(), group);
        for (int i = 0; i < request.metas_size(); i++) {
            response->add_status();
        }

        bool errMove = false;
        auto fill = [&errMove](rmdb::BatchDelDataRsp *rsp, std::vector<GroupUnit> &units, rmdb::BatchDelDataRsp *out) {
            for (size_t i = 0; i < units.size(); i++) {
                auto g = units[i];
                auto idx = g.idx;

                auto status = rsp->mutable_status(idx);
                if (!out) {
                    status->set_code(rmdb::ErrorCode::CLIENT_NOT_CONNECTED);
                } else {
                    status->CopyFrom(out->status(i));
                    if (status->code() == rmdb::ErrorCode::ERR_MOVE) {
                        errMove = true;
                    }
                }
            }
        };

        for (auto &kv: group) {
            rmdb::BatchDelDataReq in;
            for (size_t i = 0; i < kv.second.size(); i++) {
                auto meta = in.add_metas();
                meta->CopyFrom(*kv.second[i].meta);
            }

            M_GET_IMPL_CLIENT_BYSLOT(kv.first);
            if (!cli.Valid()) {
                fill(response, kv.second, nullptr);
                continue;
            }

            rmdb::BatchDelDataRsp out;
            auto status = cli.BatchDel(ctx, in, &out);
            if (!GRPC_OK(status)) {
                fill(response, kv.second, nullptr);
            } else {
                fill(response, kv.second, &out);
            }
        }

        if (errMove) {
            this->updateCluster();
        }
        return grpc::Status::OK.error_code();
    }

    int RmdbClient::BatchSetNx(rmdb::BatchSetNxDataReq &request, rmdb::BatchSetNxDataRsp *response,
                               uint32_t timeout) {
        auto slots = this->getSlots();
        if (slots == 0) {
            return (int) ::grpc::INTERNAL;
        }

        std::unordered_map<int, std::vector<GroupUnit>> group;
        groupingData(this->dbname_, slots, *request.mutable_meta_and_data(), group);
        for (int i = 0; i < request.meta_and_data_size(); i++) {
            response->add_status();
        }

        bool errMove = false;
        auto fill = [&errMove](rmdb::BatchSetNxDataRsp *rsp, std::vector<GroupUnit> &units,
                               rmdb::BatchSetNxDataRsp *out) {
            for (size_t i = 0; i < units.size(); i++) {
                auto g = units[i];
                auto idx = g.idx;

                auto status = rsp->mutable_status(idx);
                if (!out) {
                    status->set_code(rmdb::ErrorCode::CLIENT_NOT_CONNECTED);
                } else {
                    status->CopyFrom(out->status(i));
                    if (status->code() == rmdb::ErrorCode::ERR_MOVE) {
                        errMove = true;
                    }
                }
            }
        };

        for (auto &kv: group) {
            rmdb::BatchSetNxDataReq in;
            for (size_t i = 0; i < kv.second.size(); i++) {
                auto metaAndData = in.add_meta_and_data();
                metaAndData->mutable_meta()->CopyFrom(*kv.second[i].meta);
                metaAndData->set_data(kv.second[i].data, kv.second[i].dataLen);
            }

            M_GET_IMPL_CLIENT_BYSLOT(kv.first);
            if (!cli.Valid()) {
                fill(response, kv.second, nullptr);
                continue;
            }

            rmdb::BatchSetNxDataRsp out;
            auto status = cli.BatchSetNx(ctx, in, &out);
            if (!GRPC_OK(status)) {
                fill(response, kv.second, nullptr);
            } else {
                fill(response, kv.second, &out);
            }
        }

        if (errMove) {
            this->updateCluster();
        }
        return grpc::Status::OK.error_code();
    }

    int RmdbClient::BatchUpdate(rmdb::BatchUpdateDataReq &request, rmdb::BatchUpdateDataRsp *response,
                                uint32_t timeout) {
        auto slots = this->getSlots();
        if (slots == 0) {
            return (int) ::grpc::INTERNAL;
        }

        std::unordered_map<int, std::vector<GroupUnit>> group;
        groupingData(this->dbname_, slots, *request.mutable_meta_and_data(), group);
        for (int i = 0; i < request.meta_and_data_size(); i++) {
            response->add_status();
        }

        bool errMove = false;
        auto fill = [&errMove](rmdb::BatchUpdateDataRsp *rsp, std::vector<GroupUnit> &units,
                               rmdb::BatchUpdateDataRsp *out) {
            for (size_t i = 0; i < units.size(); i++) {
                auto g = units[i];
                auto idx = g.idx;

                auto status = rsp->mutable_status(idx);
                if (!out) {
                    status->set_code(rmdb::ErrorCode::CLIENT_NOT_CONNECTED);
                } else {
                    status->CopyFrom(out->status(i));
                    if (status->code() == rmdb::ErrorCode::ERR_MOVE) {
                        errMove = true;
                    }
                }
            }
        };

        for (auto &kv: group) {
            rmdb::BatchUpdateDataReq in;
            for (size_t i = 0; i < kv.second.size(); i++) {
                auto metaAndData = in.add_meta_and_data();
                metaAndData->mutable_meta()->CopyFrom(*kv.second[i].meta);
                metaAndData->set_data(kv.second[i].data, kv.second[i].dataLen);
            }

            M_GET_IMPL_CLIENT_BYSLOT(kv.first);
            if (!cli.Valid()) {
                fill(response, kv.second, nullptr);
                continue;
            }

            rmdb::BatchUpdateDataRsp out;
            auto status = cli.BatchUpdate(ctx, in, &out);
            if (!GRPC_OK(status)) {
                fill(response, kv.second, nullptr);
            } else {
                fill(response, kv.second, &out);
            }
        }

        if (errMove) {
            this->updateCluster();
        }
        return grpc::Status::OK.error_code();
    }

    int RmdbClient::GetCount(const std::string &table, int64_t *count, uint32_t timeout) {
        rmdb::GetDataCountReq req;
        rmdb::GetDataCountRsp rsp;

        req.set_db(this->dbname_);
        req.set_table(table);

        rmdb::MetaData md;
        md.set_db(this->dbname_);
        md.set_table(table);

        M_GET_IMPL_CLIENT();
        auto status = cli.GetCount(ctx, req, &rsp);
        if (!GRPC_OK(status)) {
            return status.error_code();
        }

        if (rsp.status().code() == rmdb::ErrorCode::OK) {
            *count = rsp.count();
        }

        return rsp.status().code();
    }

    int RmdbClient::GetMeta(rmdb::GetMetaReq &request, rmdb::GetMetaRsp *response, uint32_t timeout) {
        rmdb::MetaData md;
        if (request.db().empty()) {
            // 不符合设计，但为了性能，暂时这么做
            auto r = const_cast<rmdb::GetMetaReq *>(&request);
            r->set_db(this->dbname_);
        }

        md.set_table(request.table());
        md.set_db(request.db());

        M_GET_IMPL_CLIENT();
        auto status = cli.GetMeta(ctx, request, response);
        return status.error_code();
    }

/**
 * @param table
 * @param filter 灵活的查询条件
 * @param timeout
 * @return
 */
    int RmdbClient::Find(const std::string &table, void *filter, rmdb::FindDataRsp &rstResponse, uint32_t timeout) {
        if (!filter) {
            return (int) ::grpc::INTERNAL;
        }

        rmdb::FindDataReq stRequest;
        stRequest.Clear();
        stRequest.mutable_meta()->set_db(getDbName());
        stRequest.mutable_meta()->set_table(table);
        stRequest.mutable_meta()->set_type(rmdb::ContentType::BSON);

        // 填充mongo查询条件
        int iDataLen = 0;
        auto data = bsonBytes(filter, &iDataLen);
        stRequest.set_key_filter(data, iDataLen);

        // 开始查询
        ::grpc::Status status = ::grpc::Status::OK;
        rstResponse.Clear();
        for (int retry = 1; retry <= 2; retry++) {
            auto& md = stRequest.meta();
            M_GET_IMPL_CLIENT();
            status = cli.Find(ctx, stRequest, &rstResponse);
            if (!GRPC_OK(status)) {
                return status.error_code();
            }

            if (rstResponse.status().code() != rmdb::ErrorCode::ERR_MOVE) {
                break;
            }

            updateCluster();
        }

        return status.error_code();
    }

    /////////////////////////////////////////////////////////////

    static std::unordered_map<uint32_t, std::shared_ptr<RmdbClient>>& getClientMap() {
        static std::unordered_map<uint32_t, std::shared_ptr<RmdbClient>> mgr;
        return mgr;
    }

    // 创建客户端
    bool CreateClient(uint32_t id, const std::string& addr) {
        if (GetClient(id) != nullptr) {
            return false;
        }

        auto ptr = std::make_shared<RmdbClient>();
        if (!ptr->Dial(addr)) {
            return false;
        }

        auto& mgr = getClientMap();
        mgr[id] = ptr;
        return true;
    }

    // 通过加载rmdb配置创建客户端
    bool CreateFromConfig(const std::string& cfg) {
        std::fstream fs(cfg, std::fstream::in);
        if (!fs.is_open()) {
            return false;
        }

        rmdb::RmdbConfig rmdbCfg;
        ::google::protobuf::io::IstreamInputStream fileInput(&fs);
        if (!::google::protobuf::TextFormat::Parse(&fileInput, &rmdbCfg)) {
            return false;
        }

        for (int i = 0; i < rmdbCfg.units_size(); i++) {
            if (!CreateClient(rmdbCfg.units(i).id(), rmdbCfg.units(i).addr())) {
                return false;
            }
        }
        return true;
    }

    // 获取客户端
    std::shared_ptr<RmdbClient> GetClient(uint32_t id) {
        auto iter = getClientMap().find(id);
        if (iter == getClientMap().end()) {
            return nullptr;
        }
        return iter->second;
    }
}




