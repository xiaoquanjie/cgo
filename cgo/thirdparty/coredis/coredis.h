//
// Created by xiaoqj on 2024/1/9.
// co_redis是协程下的hiredis封装,对rediscontext中的文件描述符进行了hook, 是非阻塞的.
//

#pragma once

#include "redis_exception.h"

#include <string>
#include <mutex>
#include <unordered_map>
#include <list>
#include <sstream>
#include <hiredis/hiredis.h>
#include <memory>
#include <stdexcept>
#include <cgo/cgo.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <cstring>
#endif

// 检查context, 采用fd hook的方式.
#undef M_CHECK_REDIS_CONTEXT
#define M_CHECK_REDIS_CONTEXT() \
if (!ctx_ || !ctx_->ctx_) { throw std::runtime_error(M_ERR_REDIS_INVALID_CONNECTION); } cgo_hook_fd(ctx_->ctx_->fd);

// 检查返回值.
#undef M_CHECK_REDIS_REPLY
#define M_CHECK_REDIS_REPLY(reply) \
if (!reply) {ctx_->err_ = true; freeReplyObject(reply); throw std::runtime_error(M_ERR_REDIS_CONNECT_CLOSED);} \
if (reply->type == REDIS_REPLY_ERROR) { auto err = std::runtime_error(reply->str); freeReplyObject(reply); throw std::runtime_error(err); }


namespace coredis {
    struct _redisctx_ {
        redisContext*  ctx_ = nullptr;
        // 代表连接出错.
        bool err_ = false;
        std::string id_;
    };

    bool getReplyOk(redisReply* reply) {
        if (!reply) {
            return false;
        }

        if (reply->type == REDIS_REPLY_STATUS) {
            if (strcasecmp(reply->str, "OK") == 0) {
                return true;
            }
        }
        return false;
    }

    bool getReplyOkFree(redisReply* reply) {
        auto r = getReplyOk(reply);
        if (reply) {
            freeReplyObject(reply);
        }
        return r;
    }

    const char* getReplyError(redisReply* reply, const char* desc = nullptr) {
        if (!reply) {
            return desc == nullptr ? M_ERR_REDIS_NOT_DEFINED : desc;
        }

        if (reply->type == REDIS_REPLY_ERROR) {
            return reply->str;
        }
        return "";
    }

    bool checkReplyError(redisReply* reply) {
        if (!reply) {
            return true;
        }
        return reply->type == REDIS_REPLY_ERROR;
    }

    long long getReplyInteger(redisReply* reply) {
        if (reply->type == REDIS_REPLY_INTEGER) {
            return reply->integer;
        }
        return 0;
    }

    long long getReplyIntegerFree(redisReply* reply) {
        auto r = getReplyInteger(reply);
        if (reply) {
            freeReplyObject(reply);
        }
        return r;
    }

    const char* getReplyString(redisReply* reply, int& len) {
        len = 0;
        if (reply->type == REDIS_REPLY_STRING) {
            len = (int)reply->len;
            return reply->str;
        }
        return "";
    }

    void getReplyStringFree(redisReply* reply, std::string& v) {
        int len = 0;
        auto r = getReplyString(reply, len);
        if (r) {
            v.clear();
            v.append(r, len);
        }
        if (reply) {
            freeReplyObject(reply);
        }
    }

    //=====================================================
    class _redispool_ {
    private:
        struct ContextSet {
            // 已有的连接数.
            unsigned int conns = 0;
            cgo::mutex mu;
            std::list<_redisctx_*> ctxs;

            inline void lock() {
                mu.lock();
            }
            inline void unlock() {
                mu.unlock();
            }
        };

        using ContextSetPtr = std::shared_ptr<ContextSet>;

        unsigned int max_conns_ = 0;
        cgo::mutex mu_;
        std::unordered_map<std::string, ContextSetPtr> ctx_map_;

    protected:
        static std::string& CalcId(const std::string& ip, unsigned short port, const std::string& auth, unsigned short db, std::string& id) {
            id = auth + ":" + ip + ":" + std::to_string(port) + ":" + std::to_string(db);
            return id;
        }

    public:
        _redispool_() = default;

        ~_redispool_() {
            std::scoped_lock<cgo::mutex> lock(mu_);
            for (auto& kv : ctx_map_) {
                kv.second->lock();
                for (auto& ctx : kv.second->ctxs) {
                    redisFree(ctx->ctx_);
                    delete ctx;
                }
                kv.second->ctxs.clear();
                kv.second->unlock();

            }
            ctx_map_.clear();
        }

        ContextSetPtr GetContextSet(const std::string& id) {
            std::scoped_lock<cgo::mutex> lock(mu_);
            auto iter = ctx_map_.find(id);
            if (iter == ctx_map_.end()) {
                auto cs = std::make_shared<ContextSet>();
                cs->conns = 0;
                iter = ctx_map_.insert(std::make_pair(id, cs)).first;
            }
            return iter->second;
        }

        // @timeout 精度是秒.
        _redisctx_* BorrowContext(const std::string& ip, unsigned short port, const std::string& auth, unsigned short db, unsigned short timeout) {
            if (cgocoid() == -1) {
                assert(false && "not in coroutine");
                throw std::runtime_error("not in coroutine");
            }

            _redisctx_* ctx = nullptr;
            std::runtime_error err("");
            std::string id;
            CalcId(ip, port, auth, db, id);
            auto cs = GetContextSet(id);
            cs->lock();

            do {
                if (!cs->ctxs.empty()) {
                    ctx = cs->ctxs.front();
                    cs->ctxs.pop_front();
                    break;
                }

                // 超过最大连接数.
                if (max_conns_ != 0 && cs->conns >= max_conns_) {
                    if (timeout != 0) {
                        gowait(timeout*1000);
                    }
                    if (!cs->ctxs.empty()) {
                        ctx = cs->ctxs.front();
                        cs->ctxs.pop_front();
                        break;
                    }

                    err = std::runtime_error(M_ERR_REDIS_TOO_MANY_CONNECTION);
                    break;
                }

                redisContext* rctx = nullptr;

                do {
                    struct timeval tv = {timeout, 0};
                    rctx = redisConnectWithTimeout(ip.c_str(), port, tv);
                    if (!rctx) {
                        err = std::runtime_error(M_ERR_REDIS_CONNECT_FAIL);
                        break;
                    }

                    if (!auth.empty()) {
                        auto reply = (redisReply*)redisCommand(rctx, "AUTH %s", auth.c_str());
                        if (!getReplyOk(reply)) {
                            err = std::runtime_error(getReplyError(reply, M_ERR_REDIS_AUTH_FAIL));
                        }
                        freeReplyObject(reply);
                        if (strcasecmp(err.what(), "") != 0) {
                            break;
                        }
                    }

                    if (db != 0) {
                        auto reply = (redisReply*)redisCommand(rctx, "SELECT %d", db);
                        if (!getReplyOk(reply)) {
                            err = std::runtime_error(getReplyError(reply));
                        }
                        freeReplyObject(reply);
                        if (strcasecmp(err.what(), "") != 0) {
                            break;
                        }
                    }
                } while (false);

                if (strcasecmp(err.what(), "") == 0) {
                    ctx = new _redisctx_;
                    ctx->ctx_ = rctx;
                    ctx->id_ = id;
                    ctx->err_ = false;
                    cs->conns++;
                } else {
                    if (rctx) {
                        redisFree(rctx);
                    }
                }
            } while (false);

            cs->unlock();
            if (strcasecmp(err.what(), "") != 0) {
                throw std::runtime_error(err);
            }
            return ctx;
        }

        void ReturnContext(_redisctx_* ctx) {
            if (!ctx) {
                return;
            }
            assert(ctx->ctx_ != nullptr);
            auto cs = GetContextSet(ctx->id_);
            cs->lock();
            if (ctx->err_) {
                redisFree(ctx->ctx_);
                delete ctx;
                cs->conns--;
            } else {
                cs->ctxs.push_back(ctx);
            }
            cs->unlock();
        }

        void SetMaxConnection(unsigned int c) {
            max_conns_ = c;
        }
    };

    inline _redispool_* getPool() {
        static _redispool_ p;
        return &p;
    }

    //=====================================================
    class RedisConnection {
        friend class RedisPool;

    protected:
        _redisctx_* ctx_;

    protected:
        RedisConnection() {
            ctx_ = nullptr;
        }

        explicit RedisConnection(_redisctx_* ctx) : ctx_(ctx) {}

        RedisConnection& operator=(const RedisConnection&) {
            return *this;
        }
        explicit RedisConnection(const RedisConnection&) {
            // empty
        }
    public:

        ~RedisConnection() {
            if (ctx_) {
                getPool()->ReturnContext(ctx_);
            }
        }

        // 判断连接是否有效.
        // 连接无效返回真.
        bool operator!() {
            return ctx_ == nullptr;
        }

        // 判断连接是否有效.
        // 连接有效返回真.
        bool operator()() {
            return ctx_ != nullptr;
        }

        // 设置超时,秒数.
        // 返回1设置成功，返回0表示key不存在.
        long long Expire(const std::string& key, time_t expire) {
            M_CHECK_REDIS_CONTEXT();

            auto reply = (redisReply*)redisCommand(ctx_->ctx_, "EXPIRE %s %d", key.c_str(), expire);
            M_CHECK_REDIS_REPLY(reply);

            return getReplyIntegerFree(reply);
        }

        // 删除key.
        // 返回删除成功key的个数，返回0表示key不存在.
        long long Del(const std::string& key) {
            std::list<std::string> keys;
            keys.push_back(key);
            return this->Del(keys);
        }
        template<class T>
        long long  Del(const T& keys) {
            M_CHECK_REDIS_CONTEXT();

            std::string cmd = "DEL ";
            for (auto& k : keys) {
                cmd += k + " ";
            }

            auto reply = (redisReply*)redisCommand(ctx_->ctx_, cmd.c_str());
            M_CHECK_REDIS_REPLY(reply);

            return getReplyIntegerFree(reply);
        }

        // 设置string, @timeout代表毫秒.
        void Set(const std::string& key, const std::string& val, time_t timeout = 0) {
            M_CHECK_REDIS_CONTEXT();

            redisReply* reply = nullptr;
            if (timeout == 0) {
                reply = (redisReply*)redisCommand(ctx_->ctx_, "SET %s %s", key.c_str(), val.c_str());
            } else {
                reply = (redisReply*)redisCommand(ctx_->ctx_, "SET %s %s PX %d", key.c_str(), val.c_str(), timeout);
            }

            M_CHECK_REDIS_REPLY(reply);
            getReplyOkFree(reply);
        }
        template<typename T>
        void Set(const std::string& key, T value, time_t timeout = 0) {
            std::ostringstream oss;
            oss << value;
            this->Set(key, oss.str(), timeout);
        }

        // 不存在时设置string, @timeout代表毫秒.
        // 返回true表示设置成功，返回false表示设置失败.
        bool SetNx(const std::string& key, const std::string& val, time_t timeout = 0) {
            M_CHECK_REDIS_CONTEXT();

            redisReply* reply = nullptr;
            if (timeout == 0) {
                reply = (redisReply*)redisCommand(ctx_->ctx_, "SET %s %s NX", key.c_str(), val.c_str());
            } else {
                reply = (redisReply*)redisCommand(ctx_->ctx_, "SET %s %s PX %d NX", key.c_str(), val.c_str(), timeout);
            }

            M_CHECK_REDIS_REPLY(reply);
            return getReplyOkFree(reply);
        }
        template<typename T>
        bool SetNx(const std::string& key, T value, time_t timeout = 0) {
            std::ostringstream oss;
            oss << value;
            return this->SetNx(key, oss.str(), timeout);
        }

        // 存在时才设置string, @timeout代表毫秒.
        // 返回true表示设置成功，返回false表示设置失败.
        bool SetXx(const std::string& key, const std::string& val, time_t timeout = 0) {
            M_CHECK_REDIS_CONTEXT();

            redisReply* reply = nullptr;
            if (timeout == 0) {
                reply = (redisReply*)redisCommand(ctx_->ctx_, "SET %s %s XX", key.c_str(), val.c_str());
            } else {
                reply = (redisReply*)redisCommand(ctx_->ctx_, "SET %s %s PX %d XX", key.c_str(), val.c_str(), timeout);
            }

            M_CHECK_REDIS_REPLY(reply);
            return getReplyOkFree(reply);
        }
        template<typename T>
        bool SetXx(const std::string& key, T value, time_t timeout = 0) {
            std::ostringstream oss;
            oss << value;
            return this->SetXx(key, oss.str(), timeout);
        }

        // 获取string, @exist允许为空，为true表示数据存在，为false表示数据不存在.
        template<typename T>
        T& Get(const std::string& key, T* val, bool* exist = nullptr) {
            std::string v;
            bool check = false;
            this->Get(key, &v, &check);

            if (check) {
                std::istringstream iss(v);
                iss >> val;
            }
            if (exist) {
                *exist = check;
            }
            return *val;
        }
        std::string& Get(const std::string& key, std::string* val, bool* exist = nullptr) {
            M_CHECK_REDIS_CONTEXT();

            auto reply = (redisReply*)redisCommand(ctx_->ctx_, "GET %s", key.c_str());
            M_CHECK_REDIS_REPLY(reply);

            getReplyStringFree(reply, *val);
            if (exist) {
                *exist = !val->empty();
            }
            return *val;
        }


        // 设置新的string,并获取旧的string值.
        // 返回true表示设置成功，返回false表示设置失败.
        template<typename T>
        T& GetSet(const std::string& key, T val, T& oldval, bool* exist = nullptr, time_t timeout = 0) {
            std::ostringstream oss;
            oss << val;
            std::string oldvalstr;
            this->GetSet(key, oss.str(), oldvalstr, exist, timeout);
            if (!oldvalstr.empty()) {
                std::istringstream iss(oldvalstr);
                iss >> oldval;
            }
            return oldval;
        }
        std::string& GetSet(const std::string& key, const std::string& val, std::string& oldval, bool* exist = nullptr, time_t timeout = 0) {
            M_CHECK_REDIS_CONTEXT();

            redisReply* reply = nullptr;
            if (timeout == 0) {
                reply = (redisReply*)redisCommand(ctx_->ctx_, "SET %s %s GET", key.c_str(), val.c_str());
            } else {
                reply = (redisReply*)redisCommand(ctx_->ctx_, "SET %s %s GET PX %d", key.c_str(), val.c_str(), timeout);
            }

            M_CHECK_REDIS_REPLY(reply);
            getReplyStringFree(reply, oldval);
            return oldval;
        }

        // 获取字符串长度.
        long long  Strlen(const std::string& key) {
            M_CHECK_REDIS_CONTEXT();

            auto reply = (redisReply*)redisCommand(ctx_->ctx_, "STRLEN %s", key.c_str());

            M_CHECK_REDIS_REPLY(reply);
            return getReplyIntegerFree(reply);
        }
    };

    //=====================================================
    class RedisPool {
    public:
        // 设置最大连接数.
        // 默认是无限制.
        static void SetMaxConnection(unsigned int c) {
            getPool()->SetMaxConnection(c);
        }

        // @timeout 精度是秒.
        // 需要在协程中才能使用.
        static RedisConnection GetConnection(const std::string& ip, unsigned short port = 6379, unsigned short db = 0, unsigned short timeout = 3) {
            auto ctx = getPool()->BorrowContext(ip, port, "", db, timeout);
            if (ctx) {
                return RedisConnection(ctx);
            }
            return RedisConnection();
        }

        // @timeout 精度是秒.
        // 需要在协程中才能使用.
        static RedisConnection GetConnection(const std::string& ip, const std::string& auth, unsigned short port = 6379, unsigned short db = 0, unsigned short timeout = 3) {
            auto ctx = getPool()->BorrowContext(ip, port, auth, db, timeout);
            if (ctx) {
                return RedisConnection(ctx);
            }
            return RedisConnection();
        }
    };

}
