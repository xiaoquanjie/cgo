//
// Created by xiaoqj on 2023/7/10.
//

#pragma once

#include <unordered_map>
#include <mutex>
#include <atomic>
#include <memory>
#include <shared_mutex>
#include "rmdb/rmdb.pb.h"

namespace google::protobuf {
    class Message;
}

namespace rmdbclient {

    class RmdbClientImplement;

    // 此类是协程安全的.
    class RmdbClient {
    public:
        struct ClusterInfo {
            std::atomic<uint32_t> slots = 0;
            bool online = false;
            std::unordered_map<uint32_t, std::string> target;
            time_t lastUpdate = 0;
        };

        RmdbClient();

        ~RmdbClient();

        bool Dial(const std::string &target);

        /*
         * Set 强制设置新数据,无论该数据事先是否已存在.
         * */
        int Set(rmdb::SetDataReq &request, rmdb::SetDataRsp *response, uint32_t timeout = 3000);

        /*
         * SetPb 强制设置新数据,接受pb结构体参数,无论该数据事先是否已存在.
         * */
        std::pair<int, rmdb::StatusData> SetPb(const std::string &table,
                                               const std::unordered_map<std::string, std::string> &keys,
                                               const ::google::protobuf::Message &msg,
                                               uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> SetKv(const std::string &table,
                                               const std::unordered_map<std::string, std::string> &keys,
                                               const std::unordered_map<std::string, std::string> &value,
                                               uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> SetPbBinary(const std::string &table,
                                                     const std::unordered_map<std::string, std::string> &keys,
                                                     const ::google::protobuf::Message &msg,
                                                     uint32_t timeout = 3000);

        /*
         * SetString 强制设置新数据.接受普通字符串.无论该数据事先是否已存在.
         * */
        std::pair<int, rmdb::StatusData> SetString(const std::string &table,
                                                   const std::unordered_map<std::string, std::string> &keys,
                                                   const std::string &data,
                                                   uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> SetBinary(const std::string &table,
                                                   const std::unordered_map<std::string, std::string> &keys,
                                                   const char *data,
                                                   size_t dataLen,
                                                   uint32_t timeout = 3000);

        int SetNx(rmdb::SetNxDataReq &request, rmdb::SetNxDataRsp *response, uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> SetNxPb(const std::string &table,
                                                 const std::unordered_map<std::string, std::string> &keys,
                                                 const ::google::protobuf::Message &msg,
                                                 uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> SetNxKv(const std::string &table,
                                                 const std::unordered_map<std::string, std::string> &keys,
                                                 const std::unordered_map<std::string, std::string> &value,
                                                 uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> SetNxPbBinary(const std::string &table,
                                                       const std::unordered_map<std::string, std::string> &keys,
                                                       const ::google::protobuf::Message &msg,
                                                       uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> SetNxString(const std::string &table,
                                                     const std::unordered_map<std::string, std::string> &keys,
                                                     const std::string &data,
                                                     uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> SetNxBinary(const std::string &table,
                                                     const std::unordered_map<std::string, std::string> &keys,
                                                     const char *data,
                                                     size_t dataLen,
                                                     uint32_t timeout = 3000);

        /*
         * Get 获取数据.当数据不存在时,会返回ErrorCode_DATA_NOT_EXIST错误码.
         * */
        int Get(rmdb::GetDataReq &request, rmdb::GetDataRsp *response, uint32_t timeout = 3000);

        /*
         * GetPb 返回pb类型数据,pb的类型要跟之前SetPb的一致,否则会出现类型转换错误.当数据不存在时,会返回ErrorCode_DATA_NOT_EXIST错误码.
         * */
        std::pair<int, rmdb::StatusData> GetPb(const std::string &table,
                                               const std::unordered_map<std::string, std::string> &keys,
                                               ::google::protobuf::Message *msg,
                                               uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> GetKv(const std::string &table,
                                               const std::unordered_map<std::string, std::string> &keys,
                                               std::unordered_map<std::string, std::string> &value,
                                               uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> GetPbBinary(const std::string &table,
                                                     const std::unordered_map<std::string, std::string> &keys,
                                                     ::google::protobuf::Message *msg,
                                                     uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> GetBinary(const std::string &table,
                                                   const std::unordered_map<std::string, std::string> &keys,
                                                   std::string &data,
                                                   uint32_t timeout = 3000);

        /*
         * GetString 返回string数据.当数据不存在时,会返回ErrorCode_DATA_NOT_EXIST错误码.
         * */
        std::pair<int, rmdb::StatusData> GetString(const std::string &table,
                                                   const std::unordered_map<std::string, std::string> &keys,
                                                   std::string &data,
                                                   uint32_t timeout = 3000);

        int Del(rmdb::DelDataReq &request, rmdb::DelDataRsp *response, uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> DelData(const std::string &table,
                                                 const std::unordered_map<std::string, std::string> &keys,
                                                 uint32_t timeout = 3000);

        int Update(rmdb::UpdateDataReq &request, rmdb::UpdateDataRsp *response, uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> UpdatePb(const std::string &table,
                                                  const std::unordered_map<std::string, std::string> &keys,
                                                  const ::google::protobuf::Message &msg,
                                                  uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> UpdateKv(const std::string &table,
                                                  const std::unordered_map<std::string, std::string> &keys,
                                                  const std::unordered_map<std::string, std::string> &value,
                                                  uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> UpdatePbBinary(const std::string &table,
                                                        const std::unordered_map<std::string, std::string> &keys,
                                                        const ::google::protobuf::Message &msg,
                                                        uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> UpdateString(const std::string &table,
                                                      const std::unordered_map<std::string, std::string> &keys,
                                                      const std::string &data,
                                                      uint32_t timeout = 3000);

        std::pair<int, rmdb::StatusData> UpdateBinary(const std::string &table,
                                                      const std::unordered_map<std::string, std::string> &keys,
                                                      const char *data,
                                                      size_t dataLen,
                                                      uint32_t timeout = 3000);

        int BatchSet(rmdb::BatchSetDataReq &request, rmdb::BatchSetDataRsp *response, uint32_t timeout = 3000);

        int BatchGet(rmdb::BatchGetDataReq &request, rmdb::BatchGetDataRsp *response, uint32_t timeout = 3000);

        int BatchDel(rmdb::BatchDelDataReq &request, rmdb::BatchDelDataRsp *response, uint32_t timeout = 3000);

        int
        BatchSetNx(rmdb::BatchSetNxDataReq &request, rmdb::BatchSetNxDataRsp *response, uint32_t timeout = 3000);

        int BatchUpdate(rmdb::BatchUpdateDataReq &request, rmdb::BatchUpdateDataRsp *response,
                        uint32_t timeout = 3000);

        int GetCount(const std::string &table, int64_t *count, uint32_t timeout = 3000);

        int GetMeta(rmdb::GetMetaReq &request, rmdb::GetMetaRsp *response, uint32_t timeout = 3000);

        int Find(const std::string &table, void *filter, rmdb::FindDataRsp &rstResponse, uint32_t timeout = 3000);

    protected:
        int set(const std::string &table,
                const std::unordered_map<std::string, std::string> &keys,
                const char *data,
                size_t dataLen,
                rmdb::ContentType contentType,
                rmdb::SetDataRsp *response,
                uint32_t timeout);

        int setNx(const std::string &table,
                  const std::unordered_map<std::string, std::string> &keys,
                  const char *data,
                  size_t dataLen,
                  rmdb::ContentType contentType,
                  rmdb::SetNxDataRsp *response,
                  uint32_t timeout);

        int update(const std::string &table,
                   const std::unordered_map<std::string, std::string> &keys,
                   const char *data,
                   size_t dataLen,
                   rmdb::ContentType contentType,
                   rmdb::UpdateDataRsp *response,
                   uint32_t timeout);

        bool updateCluster();

        uint32_t getSlots();

        const std::string &getHost(uint32_t slot);

        const std::string &getHost(const rmdb::MetaData *meta);

        std::string &getDbName() { return dbname_; }

    protected:
        ClusterInfo cluster_;
        std::string dbname_ = "rmdb";
        std::shared_mutex mu_;
        RmdbClientImplement *impl_;
    };

    // 创建客户端.
    bool CreateClient(uint32_t id, const std::string& addr);

    // 通过加载rmdb配置创建客户端.
    bool CreateFromConfig(const std::string& cfg);

    // 获取客户端,非协程安全,需要在调用此函数之前调用CreateFromConfig/CreateClient创建好.
    typedef std::shared_ptr<RmdbClient> RmdbClientPtr;
    RmdbClientPtr GetClient(uint32_t id);

    const char* CodeMsg(rmdb::ErrorCode code);
}

// 检查返回是否成功.
#undef M_RMDB_OK
#define M_RMDB_OK(ret) \
(ret.first == 0 && ret.second.code() == rmdb::ErrorCode::OK)

// 检查数据是否不存在.
#undef M_RMDB_EXIST
#define M_RMDB_EXIST(ret) \
(ret.first == 0 && ret.second.code() == rmdb::ErrorCode::DATA_NOT_EXIST)

// 获取io错误码.
#undef M_RMDB_IOCODE
#define M_RMDB_IOCODE(ret) ret.first

// 获取错误码.
#undef M_RMDB_CODE
#define M_RMDB_CODE(ret) ret.second.code()

#undef M_RMDB_MSG
#define M_RMDB_MSG(ret) rmdbclient::CodeMsg(ret.second.code())