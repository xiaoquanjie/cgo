//
// Created by xiaoqj on 2024/1/11.
//

#pragma once

#include <mutex>
#include <unordered_map>
#include <list>
#include <functional>
#include <stdexcept>
#include <string>
#include <cgo/cgo.h>
#include <mysql.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <cstring>
#endif

// 采用fd hook的方式.
#undef M_CHECK_MYSQL_CONTEXT
#define M_CHECK_MYSQL_CONTEXT() \
if (!ctx_ || !ctx_->mysql_) { throw std::runtime_error("invalid mysql connection"); } cgo_hook_fd(ctx_->mysql_->net.fd);

#undef M_CHECK_MYSQL_ERROR
#define M_CHECK_MYSQL_ERROR() \
{                             \
    auto errn = mysql_errno(ctx_->mysql_); \
    if (errn == CR_SERVER_LOST  \
        || errn == CR_SERVER_LOST_EXTENDED \
        || errn == CR_SERVER_GONE_ERROR) { \
            ctx_->err_ = true;             \
    }                                      \
    throw std::runtime_error(mysql_error(ctx_->mysql_));  \
}

namespace comysql {
    struct _mysqlctx_ {
        MYSQL* mysql_ = nullptr;
        // 代表连接出错.
        bool err_ = false;
        std::string id_;
    };

    //=====================================================
    class _mysqlpool_ {
    private:
        struct ContextSet {
            // 已有的连接数.
            unsigned int conns = 0;
            cgo::mutex mu;
            std::list<_mysqlctx_*> ctxs;

            inline void lock() {
                mu.lock();
            }
            inline void unlock() {
                mu.unlock();
            }
        };

        using ContextSetPtr = std::shared_ptr<ContextSet>;

    protected:
        unsigned int max_conns_ = 0;
        cgo::mutex mu_;
        std::unordered_map<std::string, ContextSetPtr> ctx_map_;

        static std::string& CalcId(const std::string& host,
                            const std::string& user,
                            const std::string& pwd,
                            const std::string& db,
                            unsigned short port,
                            std::string& id) {
            id = host + ":" + user + ":" + pwd + ":" + db + ":" + std::to_string(port);
            return id;
        }

    public:
        _mysqlpool_() = default;

        ~_mysqlpool_() {
            std::scoped_lock<cgo::mutex> lock(mu_);

            for (auto& kv : ctx_map_) {
                kv.second->lock();
                for (auto& ctx : kv.second->ctxs) {
                    mysql_close(ctx->mysql_);
                    delete ctx;
                }
                kv.second->ctxs.clear();
                kv.second->unlock();
            }

            ctx_map_.clear();
        }

        void SetMaxConnection(unsigned int c) {
            max_conns_ = c;
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

        _mysqlctx_* BorrowContext(const std::string& host,
                                  const std::string& user,
                                  const std::string& pwd,
                                  const std::string& db,
                                  unsigned short port,
                                  unsigned short timeout) {
            if (cgocoid() == -1) {
                assert(false && "not in coroutine");
                throw std::runtime_error("not in coroutine");
            }

            std::string id;
            CalcId(host, user, pwd, db, port, id);
            auto cs = GetContextSet(id);
            std::runtime_error err("");
            _mysqlctx_* ctx = nullptr;
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

                    err = std::runtime_error("over mysql connection count limit");
                    break;
                }

                auto mysql = mysql_init(nullptr);
                if (!mysql) {
                    err = std::runtime_error("mysql_init error");
                    break;
                }

                // 设置连接超时,毫秒.
                timeout *= 1000;
                mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

                if (!mysql_real_connect(mysql, host.c_str(), user.c_str(), pwd.c_str(), db.c_str(), port, nullptr, 0)) {
                    err = std::runtime_error(mysql_error(mysql));
                    mysql_close(mysql);
                    break;
                }

                if (strcasecmp(err.what(), "") == 0) {
                    ctx = new _mysqlctx_;
                    ctx->mysql_ = mysql;
                    ctx->id_ = id;
                    ctx->err_ = false;
                    cs->conns++;
                }
            } while (false);

            cs->unlock();

            if (strcasecmp(err.what(), "") != 0) {
                throw std::runtime_error(err);
            }
            return ctx;
        }

        void ReturnContext(_mysqlctx_* ctx) {
            if (!ctx) {
                return;
            }

            assert(ctx->mysql_ != nullptr);
            auto cs = GetContextSet(ctx->id_);
            cs->lock();
            if (ctx->err_) {
                mysql_close(ctx->mysql_);
                delete ctx;
                cs->conns--;
            } else {
                cs->ctxs.push_back(ctx);
            }
            cs->unlock();
        }
    };

    inline _mysqlpool_* getPool() {
        static _mysqlpool_ pool;
        return &pool;
    }

    //=====================================================
    class MysqlConnection {
        friend class MysqlPool;

    protected:
        _mysqlctx_* ctx_ = nullptr;

        MysqlConnection() : ctx_(nullptr) {}

        explicit MysqlConnection(_mysqlctx_* ctx) : ctx_(ctx) {}

        MysqlConnection& operator=(const MysqlConnection&) { return *this; };
        MysqlConnection(const MysqlConnection&) {};

    public:
        ~MysqlConnection() {
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

        // 执行sql语句.
        // @timeout 精度是秒.
        // @return  影响的行数.
        unsigned long long Execute(const std::string& sql, time_t timeout = 3) {
            M_CHECK_MYSQL_CONTEXT();

            // 设置超时.
            timeout *= 1000;
            mysql_options(ctx_->mysql_, MYSQL_OPT_READ_TIMEOUT, &timeout);
            mysql_options(ctx_->mysql_, MYSQL_OPT_WRITE_TIMEOUT, &timeout);

            if (mysql_query(ctx_->mysql_, sql.c_str())) {
                M_CHECK_MYSQL_ERROR();
            }
            // 查询影响的行数.
            return mysql_affected_rows(ctx_->mysql_);
        }

        // 查询sql语句.
        // @cb是回调函数：@row代表一行数据,@row_num代表共几行,@col_num共几列,@row_idx当前是第几行,当row_idx等于row_num时遍历结束.
        void Query(const std::string& sql,
                   const std::function<void(MYSQL_ROW row,
                           unsigned long long row_num,
                           unsigned int col_num,
                           unsigned long long row_idx)>& cb,
                   time_t timeout = 3) {
            auto res = Query(sql, timeout);
            auto row_num = mysql_num_rows(res);
            auto col_num = mysql_num_fields(res);

            unsigned long long idx = 1;
            for (MYSQL_ROW row = mysql_fetch_row(res); row != nullptr; row = mysql_fetch_row(res)) {
                cb(row, row_num, col_num, idx++);
            }

            // 释放结果集.
            mysql_free_result(res);
        }

        // 查询sql语句.
        // @return 返回结果集.
        MYSQL_RES* Query(const std::string& sql, time_t timeout = 3) {
            M_CHECK_MYSQL_CONTEXT();

            // 设置超时.
            timeout *= 1000;
            mysql_options(ctx_->mysql_, MYSQL_OPT_READ_TIMEOUT, &timeout);
            mysql_options(ctx_->mysql_, MYSQL_OPT_WRITE_TIMEOUT, &timeout);

            if (mysql_query(ctx_->mysql_, sql.c_str())) {
                M_CHECK_MYSQL_ERROR();
            }

            // 一次性获取所有的结果集.
            auto res = mysql_store_result(ctx_->mysql_);
            if (!res) {
                M_CHECK_MYSQL_ERROR();
            }
            return res;
        }
    };

    //=====================================================
    class MysqlPool {
    public:
        // 设置最大连接数.
        // 默认是无限制.
        static void SetMaxConnection(unsigned int c) {
            getPool()->SetMaxConnection(c);
        }

        // @timeout 精度是秒.
        // 需要在协程中才能使用.
        static MysqlConnection GetConnection(const std::string& host,
                                             const std::string& user,
                                             const std::string& pwd,
                                             const std::string& db,
                                             unsigned short port = 3306,
                                             unsigned short timeout = 3) {
            auto ctx = getPool()->BorrowContext(host, user, pwd, db, port, timeout);
            if (ctx) {
                return MysqlConnection(ctx);
            }
            return MysqlConnection();
        }
    };
}