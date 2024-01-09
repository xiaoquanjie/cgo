//
// Created by xiaoqj on 2024/1/9.
//

#pragma once

#include <string>
#include <mutex>
#include <unordered_map>
#include <list>
#include <string.h>
#include <hiredis/hiredis.h>
#include "cgo/cgo.h"

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
        if (!reply) {
            return false;
        }

        auto r = getReplyOk(reply);
        freeReplyObject(reply);
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

    class _redispool_ {
    private:
        struct ContextSet {
            unsigned int conns; // 已有的连接数
            cgo::chan<int> ch_;
            std::list<_redisctx_*> ctxs;

            ContextSet() {
                ch_ = makechan<int>(1);
            }

            ~ContextSet() {
                if (ch_.use_count() == 1) {
                    closechan(ch_);
                }
            }

            void lock() {
                ch_ >> 1;
            }

            void unlock() {
                int t = 0;
                ch_ << t;
            }
        };

        unsigned int max_conns_ = 0;
        std::mutex mu_;
        std::unordered_map<std::string, ContextSet> ctx_map_;

    protected:
        std::string& CalcId(const std::string& ip, unsigned short port, const std::string& auth, unsigned short db, std::string& id) {
            id = auth + ":" + ip + ":" + std::to_string(port) + ":" + std::to_string(db);
            return id;
        }

    public:
        _redispool_() {

        }

        ~_redispool_() {
            std::scoped_lock<std::mutex> lock(mu_);

            for (auto& kv : ctx_map_) {
                kv.second.lock();
                for (auto& ctx : kv.second.ctxs) {
                    redisFree(ctx->ctx_);
                    delete ctx;
                }
                kv.second.ctxs.clear();
                kv.second.unlock();

            }

            ctx_map_.clear();
        }

        ContextSet* GetContextSet(const std::string& id) {
            std::scoped_lock<std::mutex> lock(mu_);
            auto iter = ctx_map_.find(id);
            if (iter == ctx_map_.end()) {
                ContextSet cs;
                cs.conns = 0;
                iter = ctx_map_.insert(std::make_pair(id, cs)).first;
            }
            ContextSet* cs = &iter->second;
            return cs;
        }

        // @timeout 精度是秒
        _redisctx_* BorrowContext(const std::string& ip, unsigned short port, const std::string& auth, unsigned short db, unsigned short timeout) {
            std::string id;
            CalcId(ip, port, auth, db, id);

            ContextSet* cs = GetContextSet(id);
            RedisException err;
            _redisctx_* ctx = nullptr;
            redisContext* rctx = nullptr;
            redisReply* reply = nullptr;

            cs->lock();

            if (!cs->ctxs.empty()) {
                ctx = cs->ctxs.front();
                cs->ctxs.pop_front();
                cs->unlock();
                return ctx;
            }

            // 超过最大连接数
            if (max_conns_ != 0 && cs->conns >= max_conns_) {
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
            cs->conns++;
            cs->unlock();
            return ctx;

rediserr:
            cs->unlock();
            if (rctx) {
                redisFree(rctx);
            }
            if (reply) {
                freeReplyObject(reply);
            }
            throw err;
        }

        void ReturnContext(_redisctx_* ctx) {
            ContextSet* cs = GetContextSet(ctx->id_);
            cs->lock();
            if (ctx->err_) {

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


    };

    class RedisPool {
    public:
        // 设置最大连接数
        // 默认是无限制
        static void SetMaxConnection(unsigned int c) {
            getPool()->SetMaxConnection(c);
        }

        // @timeout 精度是秒
        static RedisConnection GetConnection(const std::string& ip, unsigned short port = 6379, unsigned short db = 0, unsigned short timeout = 3) {
            auto ctx = getPool()->BorrowContext(ip, port, "", db, timeout);
            if (ctx) {
                return RedisConnection(ctx);
            }
            return RedisConnection();
        }

        // @timeout 精度是秒
        static RedisConnection GetConnection(const std::string& ip, const std::string& auth, unsigned short port = 6379, unsigned short db = 0, unsigned short timeout = 3) {
            auto ctx = getPool()->BorrowContext(ip, port, auth, db, timeout);
            if (ctx) {
                return RedisConnection(ctx);
            }
            return RedisConnection();
        }
    };


}
