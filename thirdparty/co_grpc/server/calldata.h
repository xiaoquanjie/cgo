//
// Created by xiaoqj on 2023/5/18.
//

#pragma once

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>
#include <list>
#include <memory>
#include <mutex>
#include "co_grpc/runner/runner.h"
#include "co_grpc/log.h"

namespace co_grpc {

    // 这里不需要虚析构函数，因为不会通过基类指针释放掉
    template<class T>
    struct ServerDataCounter {
        static uint64_t construct_count;
        static uint64_t destruct_count;
#ifdef Debug
        bool need_count = true;
#endif
        ServerDataCounter() {
#ifdef Debug
            construct_count++;
#endif
        }
        ServerDataCounter(bool n) {
#ifdef Debug
            need_count = n;
        if (need_count) {
            construct_count++;
        }
#endif
        }
#ifdef Debug
        ~ServerDataCounter() {
        if (need_count) {
            destruct_count++;
        }
    }
#endif
    };

    template<class T>
    uint64_t ServerDataCounter<T>::construct_count = 0;

    template<class T>
    uint64_t ServerDataCounter<T>::destruct_count = 0;

    class IServerData : public ServerDataCounter<IServerData> {
    public:
        enum CallStatus { CREATE, PROCESS, READING, WRITING, FINISH, CONNECT, CLOSED };

        IServerData(::grpc::ServerCompletionQueue *cq) : ServerDataCounter<IServerData>(), cq_(cq), status_(CREATE) {}

        IServerData(uint32_t s, bool need_count) : ServerDataCounter<IServerData>(need_count), status_((CallStatus)s) {}

        virtual ~IServerData() {}

        virtual void Proceed(bool shutdown, bool ok) = 0;

    protected:
        IServerData(const IServerData&) = delete;
        IServerData& operator=(const IServerData&) = delete;

    protected:
        ::grpc::ServerCompletionQueue* cq_;
        ::grpc::ServerContext ctx_;
        int status_;
    };

//////////////////////////////////////////////////////////////

    template<class Request, class Response, class Runner>
    class ServerData : public IServerData {
    public:
        typedef ::grpc::ServerAsyncResponseWriter<Response> Responder;
        typedef std::function<void(::grpc::ServerContext*,
                                   Request*,
                                   Responder*,
                                   ::grpc::CompletionQueue*,
                                   ::grpc::ServerCompletionQueue*,
                                   void*)> Method;

        typedef std::function<grpc::Status(::grpc::ServerContext*, const Request*, Response*)> ON_CALL;

        ServerData(::grpc::ServerCompletionQueue *cq, Method method, ON_CALL oncall)
                : IServerData(cq), responder_(&ctx_), method_(method), oncall_(oncall) {
            Proceed(false, true);
        }

        ~ServerData() {
        }

        void Proceed(bool shutdown, bool ok) override {
            if (shutdown) {
                delete this;
                return;
            }

            if (status_ == CREATE) {
                status_ = PROCESS;

                // As part of the initial CREATE state, we *request* that the system
                // start processing SayHello requests. In this request, "this" acts are
                // the tag uniquely identifying the request (so that different CallData
                // instances can serve different requests concurrently), in this case
                // the memory address of this CallData instance.
                method_(&ctx_, &req_, &responder_, cq_, cq_, this);
                return;
            }

            if (status_ == FINISH) {
                // Once in the FINISH state, deallocate ourselves (CallData).
                delete this;
                return;
            }

            if (status_ == PROCESS) {
                // Spawn a new CallData instance to serve new clients while we process
                // the one for this CallData. The instance will deallocate itself as
                // part of its FINISH state.
                new ServerData<Request, Response, Runner>(cq_, method_, oncall_);

                if (ok) {
                    // 成功
                    Runner()([this]() {
                        auto grpc_code = this->oncall_(&ctx_, &req_, &rsp_);
                        this->status_ = FINISH;
                        this->responder_.Finish(rsp_, grpc_code, this);
                    });
                } else {
                    // 失败
                    delete this;
                }
                return;
            }
        }

    private:
        Responder responder_;
        Method method_;
        ON_CALL oncall_;

        Request  req_;
        Response rsp_;
    };

///////////////////////////////////////////////////////////////////////////

    // 客户端流
    template<class Request, class Response, class Runner>
    class ServerStreamReader : public IServerData {
    public:
        typedef ServerStreamReader<Request, Response, Runner> Self;
        typedef ::grpc::ServerAsyncReader<Response, Request> Responder;
        typedef std::function<void(::grpc::ServerContext*,
                                   Responder*,
                                   ::grpc::CompletionQueue*,
                                   ::grpc::ServerCompletionQueue*,
                                   void*)> Method;

        // 通过::grpc::ServerContext来标识是否为同一个请求的
        typedef std::function<::grpc::Status(::grpc::ServerContext*, Self*, Response*)> ON_CALL;
        typedef std::function<void(bool, Request&)> READ_CALL;

        ServerStreamReader(::grpc::ServerCompletionQueue *cq, Method method, ON_CALL oncall)
                : IServerData(cq), responder_(&ctx_), method_(method), oncall_(oncall) {
            Proceed(false, true);
        }

        ~ServerStreamReader() {
        }

        void Proceed(bool shutdown, bool ok) override {
            if (shutdown) {
                delete this;
                return;
            }

            if (status_ == CREATE) {
                status_ = PROCESS;

                // As part of the initial CREATE state, we *request* that the system
                // start processing SayHello requests. In this request, "this" acts are
                // the tag uniquely identifying the request (so that different CallData
                // instances can serve different requests concurrently), in this case
                // the memory address of this CallData instance.
                method_(&ctx_, &responder_, cq_, cq_, this);
                return;
            }

            if (status_ == FINISH) {
                // Once in the FINISH state, deallocate ourselves (CallData).
                delete this;
                return;
            }

            if (status_ == PROCESS) {
                new ServerStreamReader<Request, Response, Runner>(cq_, method_, oncall_);

                if (ok) {
                    Runner()([this]() {
                        auto grpc_code = this->oncall_(&this->ctx_, this, &this->rsp_);
                        this->status_ = FINISH;
                        this->responder_.Finish(rsp_, grpc_code, this);
                    });
                } else {
                    delete this;
                }

                return;
            }

            if (status_ == READING) {
                auto c = readcall_;
                readcall_ = nullptr;
                c(ok, req_);
                return;
            }
        }

        // 不允许连续两次调用它，需等待callback回来后
        bool AsyncRead(READ_CALL on) {
            if (readcall_) {
                return false;
            }

            readcall_ = on;
            status_ = READING;
            req_.Clear();
            responder_.Read(&req_, this);
            return true;
        }

        // 非协程安全
        bool Read(Request* req) {
            co_grpc::CoWaiter waiter;
            auto result = std::make_shared<bool>();

            this->AsyncRead([result, waiter](bool ok, Request&) {
                *result = ok;
                waiter.Resume();
            });
            waiter.wait();
            req->Swap(&req_);
            return *result;
        }

    private:
        Responder responder_;
        Method method_;
        ON_CALL oncall_;
        READ_CALL readcall_;

        Request  req_;
        Response rsp_;
    };

//////////////////////////////////////////////////////////////////////////////////////////////////////

    // 服务器流
    template<class Request, class Response, class Runner>
    class ServerStreamWriter : public IServerData {
    public:
        typedef ServerStreamWriter<Request, Response, Runner> Self;
        typedef ::grpc::ServerAsyncWriter<Response> Responder;
        typedef std::function<void(::grpc::ServerContext*,
                                   Request*,
                                   Responder*,
                                   ::grpc::CompletionQueue*,
                                   ::grpc::ServerCompletionQueue*,
                                   void*)> Method;

        typedef std::function<::grpc::Status(::grpc::ServerContext*, const Request*, Self*)> ON_CALL;
        typedef std::function<void(bool)> WRITE_CALL;

        ServerStreamWriter(::grpc::ServerCompletionQueue *cq, Method method, ON_CALL oncall)
                : IServerData(cq), responder_(&ctx_), method_(method), oncall_(oncall) {
            Proceed(false, true);
        }

        ~ServerStreamWriter() {

        }

        void Proceed(bool shutdown, bool ok) override {
            if (shutdown) {
                delete this;
                return;
            }

            if (status_ == CREATE) {
                status_ = PROCESS;
                method_(&ctx_, &req_, &responder_, cq_, cq_, this);
                return;
            }

            if (status_ == FINISH) {
                delete this;
                return;
            }

            if (status_ == PROCESS) {
                new ServerStreamWriter<Request, Response, Runner>(cq_, method_, oncall_);

                if (ok) {
                    Runner()([this]() {
                        auto grpc_code = this->oncall_(&this->ctx_, &this->req_, this);
                        this->status_ = FINISH;
                        this->responder_.Finish(grpc_code, this);
                    });
                } else {
                    delete this;
                }
                return;
            }

            if (status_ == WRITING) {
                auto c = writecall_;
                writecall_ = nullptr;
                c(ok);
                return;
            }
        }

        // 不允许连续两次调用它，需等待callback回来后
        bool AsyncWrite(const Response& rsp, std::function<void(bool)> on) {
            if (writecall_) {
                return false;
            }

            status_ = WRITING;
            writecall_ = on;
            responder_.Write(rsp, this);
            return true;
        }

        // 非协程安全
        bool Write(const Response& rsp) {
            co_grpc::CoWaiter waiter;
            auto result = std::make_shared<bool>();

            this->AsyncWrite(rsp, [waiter, result](bool ok) {
                *result = ok;
                waiter.Resume();
            });

            waiter.wait();
            return *result;
        }

    private:
        Responder responder_;
        Method method_;
        ON_CALL oncall_;
        WRITE_CALL writecall_;

        Request  req_;
    };

///////////////////////////////////////////////////////////////////////////

    // 双流
    template<class Request, class Response, class Runner>
    class ServerStreamReaderWriter : public ServerDataCounter<IServerData> {
    public:
        typedef ServerStreamReaderWriter<Request, Response, Runner> Self;
        typedef ::grpc::ServerAsyncReaderWriter<Response, Request> Responder;
        typedef std::function<void(::grpc::ServerContext*,
                                   Responder*,
                                   ::grpc::CompletionQueue*,
                                   ::grpc::ServerCompletionQueue*,
                                   void*)> Method;

        typedef std::function<void(::grpc::ServerContext*, Self*)> ON_CALL;
        typedef std::function<void(bool)> READ_CALL;

        struct CallInfo : public IServerData {
            CallInfo(Self* f, int s)
                    : IServerData(s, false), father(f) {}

            void Proceed(bool shutdown, bool ok) override {
                father->Proceed(shutdown, ok, this);
            }

            uint32_t Status() {
                return (uint32_t)status_;
            }

            Self* father = 0;
        };

        // 运行数据
        struct Data {
            Data(Self* f)
                    : responder(&ctx)
                    , connect(f, IServerData::CONNECT)
                    , read(f, IServerData::READING)
                    , close(f, IServerData::CLOSED)
                    , write(f, IServerData::WRITING)
                    , finish(f, IServerData::FINISH) {
            }

            ::grpc::ServerCompletionQueue* cq;
            ::grpc::ServerContext ctx;
            Responder responder;
            Method method;
            ON_CALL oncall;

            Request req;

            CallInfo connect;
            CallInfo read;
            CallInfo close;
            CallInfo write;
            CallInfo finish;

            int closed_ = 0; // 0表示未关闭，1表示即将关闭, 2表示已关闭
            bool writing_flag_ = false;

            bool rok_ = false;
            uint64_t rcoid_ = 0;
        };

        ServerStreamReaderWriter(::grpc::ServerCompletionQueue *cq, Method method, ON_CALL oncall)
                : ServerDataCounter<IServerData>() {
            data_ = std::make_shared<Data>(this);
            data_->cq = cq;
            data_->method = method;
            data_->oncall = oncall;

            data_->method(&data_->ctx, &data_->responder, data_->cq, data_->cq, &data_->connect);
            data_->ctx.AsyncNotifyWhenDone(&data_->close);
        }

        ~ServerStreamReaderWriter() {
            for (auto rsp :rsp_list_) {
                delete rsp;
            }
        }

        // 保证closecall最后被回调
        void Proceed(bool shutdown, bool ok, CallInfo* info) {
            if (shutdown) {
                delete this;
                return;
            }

            if (info->Status() == IServerData::CONNECT) {
                new ServerStreamReaderWriter<Request, Response, Runner>(data_->cq, data_->method, data_->oncall);
                // onconnect
                if (ok) {
                    Runner()([this]() {
                        this->data_->oncall(&this->data_->ctx, this);
                        this->_TryClose();
                    });
                } else {
                    delete this;
                }
            } else if (info->Status() == IServerData::READING) {
                data_->rok_ = ok;
                co_grpc::CoWaiter(data_->rcoid_).Resume();
            }
            else if (info->Status() == IServerData::WRITING) {
                this->mu_.lock();
                // 一直将缓存发完, 不完美凑合用
                if (rsp_list_.empty()) {
                    data_->writing_flag_ = false;
                    this->mu_.unlock();
                    if (data_->closed_ > 0) {
                        _TryClose();
                    }
                } else {
                    _TryWrite();
                    this->mu_.unlock();
                }
            } else if (info->Status() == IServerData::CLOSED) {
                // 可以忽视此错误
                //log("close over");
                //_TryClose();
            }
            else if (info->Status() == IServerData::FINISH) {
                //log("finish");
                delete this;
            } else {
                assert(false);
            }
        }

        // 写数据
        // 大于0表示成功发到缓存
        // 等于0表示发到缓存失败
        // 小于0表示链接被关闭了
        int Write(const Response& rsp) {
            std::unique_lock<std::mutex> lock(mu_);
            if (data_->closed_ > 0) {
                return -1;
            }
            if (rsp_count_ >= 20000) {
                return 0;
            }

            rsp_count_ += 1;
            auto nrsp = new Response;
            nrsp->CopyFrom(rsp);
            rsp_list_.push_back(nrsp);

            if (!data_->writing_flag_) {
                data_->writing_flag_ = true;
                _TryWrite();
            }
            return 1;
        }

        // 非协程安全接口，同时只能有一个协程在调用
        // 读失败表示链接断开
        bool Read(Request* req) {
            co_grpc::CoWaiter waiter;
            data_->rcoid_ = waiter.co_id_;
            data_->rok_ = false;
            this->data_->req.Clear();
            data_->responder.Read(&data_->req, &data_->read);
            waiter.wait();

            if (data_->rok_) {
                req->Swap(&this->data_->req);
            }
            return data_->rok_;
        }

    protected:
        void _TryWrite() {
            assert(this->rsp_list_.empty() == false);
            auto rsp = rsp_list_.front();
            rsp_list_.pop_front();
            rsp_count_ -= 1;

            data_->responder.Write(*rsp, &data_->write);
            delete rsp;
        }

        void _TryClose() {
            std::unique_lock<std::mutex> lock(mu_);
            if (data_->closed_ == 2) {
                return;
            }

            data_->closed_ = 1;
            if (data_->writing_flag_) {
                return;
            }

            data_->closed_ = 2;
            // 发起finish
            data_->responder.Finish(grpc::Status::CANCELLED, &data_->finish);
        }

    private:
        std::shared_ptr<Data> data_;
        std::list<Response*> rsp_list_;
        int rsp_count_ = 0;
        std::mutex mu_;
    };

//////////////////////////////////////////////////////////////////////////////////////////////////////

    inline uint64_t GetServerDataConstructCount() {
        return ServerDataCounter<IServerData>::construct_count;
    }

    inline uint64_t GetCServerDataDestructCount() {
        return ServerDataCounter<IServerData>::destruct_count;
    }

    inline void ServerStatistics() {
#ifdef Debug
        static time_t last_time = time(0);
    time_t now = time(0);
    if (now - last_time >= 60) {
        last_time = now;
        log("server construct count: %d", GetServerDataConstructCount());
        log("server destruct  count: %d", GetCServerDataDestructCount());
    }
#endif
    }

}
