//
// Created by xiaoqj on 2024/1/9.
//

#pragma once

#include <string>
#include <mutex>
#include <unordered_map>
#include <list>
#include <string.h>
#include <sstream>
#include <hiredis/hiredis.h>
#include <memory>
#include "cgo/cgo.h"

// 检查context, 采用fd hook的方式
#undef M_CHECK_REDIS_CONTEXT
#define M_CHECK_REDIS_CONTEXT() \
if (!ctx_ || !ctx_->ctx_) throw RedisException(M_ERR_REDIS_INVALID_CONNECTION); cgo_hook_fd(ctx_->ctx_->fd);

// 检查返回值
#undef M_CHECK_REDIS_REPLY
#define M_CHECK_REDIS_REPLY(reply) \
if (!reply) {ctx_->err_ = true; err = RedisException(M_ERR_REDIS_CONNECT_CLOSED); freeReplyObject(reply); throw err;} \
if (reply->type == REDIS_REPLY_ERROR) { err = RedisException(reply->str); freeReplyObject(reply); throw err; }


namespace co_redis {
    struct _redisctx_ {
        redisContext*  ctx_;
        bool err_;              // 代表连接出错
        std::string    id_;
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

    const char* getReplyError(redisReply* reply, const char* desc = 0) {
        if (!reply) {
            return desc == 0 ? M_ERR_REDIS_NOT_DEFINED : desc;
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
            len = reply->len;
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
            unsigned int conns; // 已有的连接数
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
        std::string& CalcId(const std::string& ip, unsigned short port, const std::string& auth, unsigned short db, std::string& id) {
            id = auth + ":" + ip + ":" + std::to_string(port) + ":" + std::to_string(db);
            return id;
        }

    public:
        _redispool_() {}

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

        // @timeout 精度是秒
        _redisctx_* BorrowContext(const std::string& ip, unsigned short port, const std::string& auth, unsigned short db, unsigned short timeout) {
            if (cgocoid() == -1) {
                assert(false && "not in coroutine");
                throw RedisException("not in coroutine");
            }

            std::string id;
            CalcId(ip, port, auth, db, id);

            auto cs = GetContextSet(id);
            RedisException err;
            _redisctx_* ctx = nullptr;
            redisContext* rctx = nullptr;
            redisReply* reply = nullptr;

            cs->lock();

            if (!cs->ctxs.empty()) {
                ctx = cs->ctxs.front();
                cs->ctxs.pop_front();
                goto redisok;
            }

            // 超过最大连接数
            if (max_conns_ != 0 && cs->conns >= max_conns_) {
                if (timeout != 0) {
                    gowait(timeout*1000);
                }
                if (!cs->ctxs.empty()) {
                    ctx = cs->ctxs.front();
                    cs->ctxs.pop_front();
                    goto redisok;
                }

                err = RedisException(M_ERR_REDIS_TOO_MANY_CONNECTION);
                goto rediserr;
            }

            struct timeval tv;
            tv.tv_sec = timeout;
            tv.tv_usec = 0;
            rctx = redisConnectWithTimeout(ip.c_str(), port, tv);
            if (!rctx) {
                err = RedisException(M_ERR_REDIS_CONNECT_FAIL);
                goto rediserr;
            }

            if (!auth.empty()) {
                reply = (redisReply*)redisCommand(rctx, "AUTH %s", auth.c_str());
                if (!getReplyOk(reply)) {
                    err = RedisException(getReplyError(reply, M_ERR_REDIS_AUTH_FAIL));
                    goto rediserr;
                }
                freeReplyObject(reply);
            }

            if (db != 0) {
                reply = (redisReply*)redisCommand(rctx, "SELECT %d", db);
                if (!getReplyOk(reply)) {
                    err = RedisException(getReplyError(reply));
                    goto rediserr;
                }
                freeReplyObject(reply);
            }

            ctx = new _redisctx_;
            ctx->ctx_ = rctx;
            ctx->id_ = id;
            ctx->err_ = false;
            cs->conns++;
            goto redisok;

rediserr:
            cs->unlock();
            if (rctx) {
                redisFree(rctx);
            }
            if (reply) {
                freeReplyObject(reply);
            }
            throw err;

redisok:
            cs->unlock();
            return ctx;
        }

        void ReturnContext(_redisctx_* ctx) {
            if (!ctx) {
                return;
            }
            assert(ctx->ctx_ != 0);
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

        RedisConnection(_redisctx_* ctx) : ctx_(ctx) {}

        RedisConnection& operator=(const RedisConnection&) = delete;

        RedisConnection(const RedisConnection&) = delete;
    public:
        ~RedisConnection() {
            if (ctx_) {
                getPool()->ReturnContext(ctx_);
            }
        }

        // 判断连接是否有效
        // 连接无效返回真
        bool operator!() {
            return ctx_ == nullptr;
        }

        // 判断连接是否有效
        // 连接有效返回真
        bool operator()() {
            return ctx_ != nullptr;
        }

        // 设置超时,秒数
        // 返回1设置成功，返回0表示key不存在
        long long Expire(const std::string& key, time_t expire) {
            return this->Expire(key.c_str(), expire);
        }
        long long Expire(const char* key, time_t expire) {
            M_CHECK_REDIS_CONTEXT();

            RedisException err;
            redisReply* reply = (redisReply*)redisCommand(ctx_->ctx_, "EXPIRE %s %d", key, expire);
            M_CHECK_REDIS_REPLY(reply);

            return getReplyIntegerFree(reply);
        }

        // 删除key
        // 返回删除成功key的个数，返回0表示key不存在
        long long Del(const std::string& key) {
            std::list<std::string> keys;
            keys.push_back(key);
            return this->Del(keys);
        }
        long long  Del(const char* key) {
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

            RedisException err;
            redisReply* reply = (redisReply*)redisCommand(ctx_->ctx_, cmd.c_str());
            M_CHECK_REDIS_REPLY(reply);

            return getReplyIntegerFree(reply);
        }

        // 设置string, @timeout代表毫秒
        void Set(const std::string& key, const std::string& val, time_t timeout = 0) {
            this->Set(key.c_str(), val.c_str(), timeout);
        }
        template<typename T>
        void Set(const char* key, T value, time_t timeout = 0) {
            std::ostringstream oss;
            oss << value;
            this->Set(key, oss.str().c_str(), timeout);
        }
        void Set(const char* key, const char* value, time_t timeout = 0) {
            M_CHECK_REDIS_CONTEXT();

            RedisException err;
            redisReply* reply;

            if (timeout == 0) {
                reply = (redisReply*)redisCommand(ctx_->ctx_, "SET %s %s", key, value);
            } else {
                reply = (redisReply*)redisCommand(ctx_->ctx_, "SET %s %s PX %d", key, value, timeout);
            }

            M_CHECK_REDIS_REPLY(reply);
            getReplyOkFree(reply);
        }

        // 不存在时设置string, @timeout代表毫秒
        // 返回true表示设置成功，返回false表示设置失败
        bool SetNx(const std::string& key, const std::string& val, time_t timeout = 0) {
            return this->SetNx(key, val, timeout);
        }
        template<typename T>
        bool SetNx(const char* key, T value, time_t timeout = 0) {
            std::ostringstream oss;
            oss << value;
            return this->SetNx(key, oss.str().c_str(), timeout);
        }
        bool SetNx(const char* key, const char* value, time_t timeout = 0) {
            M_CHECK_REDIS_CONTEXT();

            RedisException err;
            redisReply* reply;

            if (timeout == 0) {
                reply = (redisReply*)redisCommand(ctx_->ctx_, "SET %s %s NX", key, value);
            } else {
                reply = (redisReply*)redisCommand(ctx_->ctx_, "SET %s %s PX %d NX", key, value, timeout);
            }

            M_CHECK_REDIS_REPLY(reply);
            return getReplyOkFree(reply);
        }

        // 存在时才设置string, @timeout代表毫秒
        // 返回true表示设置成功，返回false表示设置失败
        bool SetXx(const std::string& key, const std::string& val, time_t timeout = 0) {
            return this->SetXx(key, val, timeout);
        }
        template<typename T>
        bool SetXx(const char* key, T value, time_t timeout = 0) {
            std::ostringstream oss;
            oss << value;
            return this->SetXx(key, oss.str().c_str(), timeout);
        }
        bool SetXx(const char* key, const char* value, time_t timeout = 0) {
            M_CHECK_REDIS_CONTEXT();

            RedisException err;
            redisReply* reply;

            if (timeout == 0) {
                reply = (redisReply*)redisCommand(ctx_->ctx_, "SET %s %s XX", key, value);
            } else {
                reply = (redisReply*)redisCommand(ctx_->ctx_, "SET %s %s PX %d XX", key, value, timeout);
            }

            M_CHECK_REDIS_REPLY(reply);
            return getReplyOkFree(reply);
        }

        // 获取string, @exist允许为空，为true表示数据存在，为false表示数据不存在
        template<typename T>
        T& Get(const char* key, T& val, bool* exist = 0) {
            std::string v;
            bool check = false;
            this->Get(key, v, &check);

            if (check) {
                std::istringstream iss(v);
                iss >> val;
            }
            if (exist) {
                *exist = check;
            }
            return val;
        }
        std::string& Get(const std::string& key, std::string& val, bool* exist = 0) {
            return this->Get(key.c_str(), val, exist);
        }
        std::string& Get(const char* key, std::string& val, bool* exist = 0) {
            M_CHECK_REDIS_CONTEXT();

            RedisException err;
            redisReply* reply = (redisReply*)redisCommand(ctx_->ctx_, "GET %s", key);
            M_CHECK_REDIS_REPLY(reply);

            getReplyStringFree(reply, val);
            if (exist) {
                *exist = !val.empty();
            }
            return val;
        }

        // 设置新的string,并获取旧的string值
        // 返回true表示设置成功，返回false表示设置失败
        template<typename T>
        T& GetSet(const std::string& key, T val, T& oldval, bool* exist = 0, time_t timeout = 0) {
            std::ostringstream oss;
            oss << val;
            std::string oldvalstr;
            this->GetSet(key, oss.str().c_str(), oldvalstr, exist, timeout);
            if (!oldvalstr.empty()) {
                std::istringstream iss(oldvalstr);
                iss >> oldval;
            }
            return oldval;
        }
        std::string& GetSet(const std::string& key, const std::string& val, std::string& oldval, bool* exist = 0, time_t timeout = 0) {
            M_CHECK_REDIS_CONTEXT();

            RedisException err;
            redisReply* reply;

            if (timeout == 0) {
                reply = (redisReply*)redisCommand(ctx_->ctx_, "SET %s %s GET", key.c_str(), val.c_str());
            } else {
                reply = (redisReply*)redisCommand(ctx_->ctx_, "SET %s %s GET PX %d", key.c_str(), val.c_str(), timeout);
            }

            M_CHECK_REDIS_REPLY(reply);
            getReplyStringFree(reply, oldval);
            return oldval;
        }

        // 获取字符串长度
        long long  Strlen(const std::string& key) {
            return this->Strlen(key.c_str());
        }
        long long  Strlen(const char* key) {
            M_CHECK_REDIS_CONTEXT();

            RedisException err;
            redisReply* reply = (redisReply*)redisCommand(ctx_->ctx_, "STRLEN %s", key);

            M_CHECK_REDIS_REPLY(reply);
            return getReplyIntegerFree(reply);
        }
    };

    //=====================================================
    class RedisPool {
    public:
        // 设置最大连接数
        // 默认是无限制
        static void SetMaxConnection(unsigned int c) {
            getPool()->SetMaxConnection(c);
        }

        // @timeout 精度是秒
        // 需要在协程中才能使用
        static RedisConnection GetConnection(const std::string& ip, unsigned short port = 6379, unsigned short db = 0, unsigned short timeout = 3) {
            auto ctx = getPool()->BorrowContext(ip, port, "", db, timeout);
            if (ctx) {
                return RedisConnection(ctx);
            }
            return RedisConnection();
        }

        // @timeout 精度是秒
        // 需要在协程中才能使用
        static RedisConnection GetConnection(const std::string& ip, const std::string& auth, unsigned short port = 6379, unsigned short db = 0, unsigned short timeout = 3) {
            auto ctx = getPool()->BorrowContext(ip, port, auth, db, timeout);
            if (ctx) {
                return RedisConnection(ctx);
            }
            return RedisConnection();
        }
    };


}
