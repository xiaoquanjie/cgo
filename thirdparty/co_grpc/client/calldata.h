//
// Created by xiaoqj on 2023/5/18.
//

#pragma once

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <list>
#include <mutex>
#include "co_grpc/log.h"
#include "co_grpc/runner/runner.h"

namespace co_grpc {

// 这里不需要虚析构函数，因为不会通过基类指针释放掉
template<class T>
struct ClientDataCounter {
    static uint64_t construct_count;
    static uint64_t destruct_count;
#ifdef Debug
    bool need_count = true;
#endif
    ClientDataCounter() {
#ifdef Debug
        construct_count++;
#endif
    }
    ClientDataCounter(bool n) {
#ifdef Debug
            need_count = n;
            if (need_count) {
                construct_count++;
            }
#endif
        }
#ifdef Debug
    ~ClientDataCounter() {
        if (need_count) {
            destruct_count++;
        }
    }
#endif
};

template<class T>
uint64_t ClientDataCounter<T>::construct_count = 0;

template<class T>
uint64_t ClientDataCounter<T>::destruct_count = 0;

class IClientData : public ClientDataCounter<IClientData> {
public:
    enum CallStatus { CREATE, PROCESS, READING, WRITING, WRITESDONE, FINISH, CONNECT };

    typedef std::shared_ptr<::grpc::ClientContext> Context;

    IClientData(Context ctx) : ClientDataCounter<IClientData>(), ctx_(ctx), status_(CREATE) {}

    IClientData(uint32_t s, bool need_count) : ClientDataCounter<IClientData>(need_count), status_(s) {

    }

    virtual ~IClientData() {}

    virtual void Proceed(bool shutdown, bool ok) = 0;

protected:
    IClientData(const IClientData&) = delete;
    IClientData& operator=(const IClientData&) = delete;

protected:
    Context ctx_;
    int status_;
};

template<class Request, class Response>
class ClientData : public IClientData {
public:
    typedef std::unique_ptr<::grpc::ClientAsyncResponseReader<Response>> Responder;

    typedef std::function<void(::grpc::Status, Response&)> ON_CALL;

    ClientData(Context ctx, Responder responder, ON_CALL oncall)
        : IClientData(ctx), responder_(std::move(responder)), oncall_(oncall) {
        Proceed(false, true);
    }

    ~ClientData() {
        // for test
        // log("delete clientdata");
    }

    void Proceed(bool shutdown, bool ok) override {
        if (this->status_ == CREATE) {
            // StartCall initiates the RPC call
            this->responder_->StartCall();

            // Request that, upon completion of the RPC, "reply" be updated with the
            // server's response; "status" with the indication of whether the operation
            // was successful. Tag the request with the memory address of the call
            // object.
            this->responder_->Finish(&this->rsp_, &this->grpc_status_, this);

            this->status_ = FINISH;
            return;
        }

        if (this->status_ == FINISH) {
            this->oncall_(this->grpc_status_, this->rsp_);
            delete this;
            return;
        }
    }

private:
    Responder responder_;
    ON_CALL oncall_;

    ::grpc::Status grpc_status_;
    Response rsp_;
};

// 此对象只能在堆上, 使用步骤是write,writedone,finish
template<class Request, class Response>
class ClientStreamWriter : public IClientData {
public:
    typedef Request RequestType;
    typedef Response ResponseType;

    typedef std::unique_ptr<::grpc::ClientAsyncWriter<Request>> Responder;
    typedef std::function<Responder(::grpc::ClientContext*,
                                    Response*,
                                    ::grpc::CompletionQueue*)> Method;
    typedef std::function<void(bool)> WRITE_CALL;
    typedef std::function<void(::grpc::Status, Response&)> FINISH_CALL;

    ClientStreamWriter(::grpc::CompletionQueue* cq, Context ctx, Method method)
            : IClientData(ctx) {
        responder_ = std::move(method(ctx.get(), &rsp_, cq));
    }

    ~ClientStreamWriter() {
        // for test
        // log("delete this");
    }

    void Proceed(bool shutdown, bool ok) override {
        if (this->status_ == CREATE) {
            return;
        }
        if (this->status_ == PROCESS) {
            startcall_(ok);
            return;
        }
        if (this->status_ == WRITING) {
            auto c = this->writecall_;
            this->writecall_ = nullptr;
            c(ok);
            return;
        }
        if (this->status_ == WRITESDONE) {
            writedonecall_(ok);
            return;
        }
        if (this->status_ == FINISH) {
            this->finishcall_(this->grpc_status_, this->rsp_);
            return;
        }
    }

    // 只需调用一次，需等待callback回来后
    bool AsyncStart(WRITE_CALL on) {
        if (this->startcall_) {
            return false;
        }

        this->status_ = PROCESS;
        this->startcall_ = on;
        // StartCall initiates the RPC call
        this->responder_->StartCall(this);
        return true;
    }

    // 不允许连续两次调用它，需等待callback回来后
    bool AsyncWrite(const Request& req, WRITE_CALL on) {
        if (this->finishcall_) {
            return false;
        }
        if (this->writecall_) {
            return false;
        }

        this->status_ = WRITING;
        this->writecall_ = on;
        this->responder_->Write(req, this);
        return true;
    }

    // 只需调用一次
    bool AsyncFinish(FINISH_CALL on) {
        if (this->finishcall_) {
            return false;
        }

        this->status_ = FINISH;
        this->finishcall_ = on;
        this->grpc_status_ = ::grpc::Status::OK;
        this->responder_->Finish(&this->grpc_status_, this);
        // log("finish");
        return true;
    }

    // 只需调用一次
    bool AsyncWritesDone(WRITE_CALL on) {
        if (this->writedonecall_) {
            return false;
        }

        this->writedonecall_ = on;
        this->status_ = WRITESDONE;
        this->responder_->WritesDone(this);
        return true;
    }

    bool OnceStart() const {
        return this->startcall_ != nullptr;
    }

    bool OnceWritesDone() const {
        return this->writedonecall_ != nullptr;
    }

    bool OnceFinish() const {
        return this->finishcall_ != nullptr;
    }

protected:
    Responder responder_;
    WRITE_CALL writecall_;
    FINISH_CALL finishcall_;
    WRITE_CALL startcall_;
    WRITE_CALL writedonecall_;

    ::grpc::Status grpc_status_;
    Response rsp_;
};

template<class Request, class Response>
class CoClientStreamWriter {
    //typedef ClientStreamWriter<Request, Response> Base;
    typedef typename ClientStreamWriter<Request, Response>::Method Method;
    typedef typename ClientStreamWriter<Request, Response>::Context Context;

protected:
    ClientStreamWriter<Request, Response> writer_;
    Response* rsp_;

public:
    CoClientStreamWriter(::grpc::CompletionQueue* cq, Context ctx, Method method, Response* rsp)
        : writer_(cq, ctx, method), rsp_(rsp) {}

    // 非协程安全接口，同时只能有一个协程在调用
    bool Write(const Request& req) {
        if (writer_.OnceFinish()) {
            return false;
        }

        if (!writer_.OnceStart()) {
            if (!Start()) {
                return false;
            }
        }

        co_grpc::CoWaiter waiter;
        auto cb_ok = std::make_shared<bool>();

        // 回调不应比waiter.wait执行的还要快
        this->writer_.AsyncWrite(req, [cb_ok, waiter](bool ok) {
            *cb_ok = ok;
            waiter.Resume();
        });

        waiter.wait(nullptr);
        return *cb_ok;
    }

    // 非协程安全接口，同时只能有一个协程在调用
    ::grpc::Status Finish() {
        if (writer_.OnceFinish()) {
            return ::grpc::Status::CANCELLED;
        }
        if (!writer_.OnceWritesDone()) {
            if (!WritesDone()) {
                return ::grpc::Status::CANCELLED;
            }
        }

        co_grpc::CoWaiter waiter;
        auto cb_status = std::make_shared<::grpc::Status>();
        auto cb_res = std::make_shared<Response>();

        waiter.wait([this, waiter, cb_status, cb_res] {
            writer_.AsyncFinish([waiter, cb_status, cb_res](::grpc::Status s, Response& res) {
                *cb_status = s;
                cb_res->Swap(&res);
                waiter.Resume();
            });
        });

        rsp_->Swap(cb_res.get());
        return *cb_status;
    }

private:
    // 只需调用一次
    bool Start() {
        if (writer_.OnceStart()) {
            return false;
        }

        co_grpc::CoWaiter waiter;
        auto cb_ok = std::make_shared<bool>();

        waiter.wait([this, cb_ok, waiter] {
            this->writer_.AsyncStart([cb_ok, waiter](bool ok) {
                *cb_ok = ok;
                waiter.Resume();
            });

        });

        return *cb_ok;
    }

    // 协程写结束，只能在协程里调用
    bool WritesDone() {
        if (writer_.OnceWritesDone()) {
            return false;
        }

        co_grpc::CoWaiter waiter;
        auto cb_ok = std::make_shared<bool>();

        waiter.wait([this, cb_ok, waiter]() {
            writer_.AsyncWritesDone([cb_ok, waiter](bool ok) {
                *cb_ok = ok;
                waiter.Resume();
            });
        });
        return *cb_ok;
    }
};

/////////////////////////////////////////////////////////////////

template<class Request, class Response>
class ClientStreamReader : public IClientData {
public:
    typedef Request RequestType;
    typedef Response ResponseType;

    typedef std::shared_ptr<ClientStreamReader<Request, Response>> Self;
    typedef std::unique_ptr<::grpc::ClientAsyncReader<Response>> Responder;
    typedef std::function<Responder(::grpc::ClientContext*,
                                    const Request&,
                                    ::grpc::CompletionQueue*)> Method;
    typedef std::function<void(Self)> ON_CALL;
    typedef std::function<void(bool, Response&)> READ_CALL;
    typedef std::function<void(::grpc::Status)> FINISH_CALL;
    typedef std::function<void(bool)> START_CALL;

    ClientStreamReader(Context ctx, Responder responder)
        : IClientData(ctx), responder_(std::move(responder)) {
        Proceed(false, true);
    }

    void Proceed(bool shutdown, bool ok) override {
        if (this->status_ == CREATE) {
            return;
        }

        if (this->status_ == PROCESS) {
            this->startcall_(ok);
            return;
        }

        if (this->status_ == READING) {
            auto c = this->readcall_;
            this->readcall_ = nullptr;
            c(ok, rsp_);
            return;
        }

        if (this->status_ == FINISH) {
            this->finishcall_(this->grpc_status_);
            return;
        }
    }

    // 只需调用一次
    bool AsyncStart(START_CALL on) {
        if (this->startcall_) {
            return false;
        }

        this->startcall_ = on;
        this->status_ = PROCESS;
        this->responder_->StartCall(this);
        return true;
    }

    // 不允许连续两次调用它，需等待callback回来后
    bool AsyncRead(READ_CALL on) {
        if (this->finishcall_) {
            return false;
        }
        if (this->readcall_) {
            return false;
        }

        this->readcall_ = on;
        this->status_ = READING;
        this->rsp_.Clear();
        this->responder_->Read(&this->rsp_, this);
        return true;
    }

    // 只需调用一次
    bool AsyncFinish(FINISH_CALL on) {
        if (this->finishcall_) {
            return false;
        }

        this->status_ = FINISH;
        this->finishcall_ = on;
        this->responder_->Finish(&this->grpc_status_, this);
        return true;
    }

    bool OnceStart() const {
        return this->startcall_ != nullptr;
    }

    bool OnceFinish() const {
        return this->finishcall_ != nullptr;
    }

protected:
    Responder responder_;
    READ_CALL readcall_;
    FINISH_CALL finishcall_;
    START_CALL startcall_;

    ::grpc::Status grpc_status_;
    Response rsp_;
};

template<class Request, class Response>
class CoClientStreamReader {
    typedef typename ClientStreamReader<Request, Response>::Responder Responder;
    typedef typename ClientStreamReader<Request, Response>::Context Context;

protected:
    ClientStreamReader<Request, Response> reader_;

public:
    CoClientStreamReader(Context ctx, Responder responder)
        : reader_(ctx, std::move(responder)) {}

    // 非协程安全接口，同时只能有一个协程在调用
    bool Read(Response* rsp) {
        if (reader_.OnceFinish()) {
            return false;
        }
        if (!reader_.OnceStart()) {
            if (!Start()) {
                return false;
            }
        }

        co_grpc::CoWaiter waiter;
        auto cb_ok = std::make_shared<bool>();
        auto cb_rsp = std::make_shared<Response>();

        waiter.wait([this, waiter, cb_ok, cb_rsp]() {
            reader_.AsyncRead([waiter, cb_ok, cb_rsp](bool ok, Response& rsp) {
                *cb_ok = ok;
                cb_rsp->Swap(&rsp);
                waiter.Resume();
            });
        });

        rsp->Swap(cb_rsp.get());
        return *cb_ok;
    }

    // 非协程安全接口，同时只能有一个协程在调用
    ::grpc::Status Finish() {
        if (reader_.OnceFinish()) {
            return ::grpc::Status::CANCELLED;
        }

        co_grpc::CoWaiter waiter;
        auto cb_status = std::make_shared<::grpc::Status>();

        waiter.wait([this, waiter, cb_status] {
            reader_.AsyncFinish([waiter, cb_status](::grpc::Status s) {
                *cb_status = s;
                waiter.Resume();
            });
        });

        return *cb_status;
    }

protected:
    bool Start() {
        if (reader_.OnceStart()) {
            return false;
        }

        co_grpc::CoWaiter waiter;
        auto cb_ok = std::make_shared<bool>();

        waiter.wait([this, waiter, cb_ok]() {
            reader_.AsyncStart([waiter, cb_ok](bool ok) {
                *cb_ok = ok;
                waiter.Resume();
            });
        });

        return *cb_ok;
    }
};

/////////////////////////////////////////////////////////////////

template<class Request, class Response>
class ClientStreamReaderWriter :
        public std::enable_shared_from_this<ClientStreamReaderWriter<Request, Response>>,
        public ClientDataCounter<IClientData> {
public:
    typedef Request RequestType;
    typedef Response ResponseType;

    typedef std::shared_ptr<ClientStreamReaderWriter<Request, Response>> Self;
    typedef std::unique_ptr<::grpc::ClientAsyncReaderWriter<Request, Response>> Responder;

    typedef std::shared_ptr<::grpc::ClientContext> Context;

    typedef std::function<void(bool)> START_CALL;
    typedef START_CALL WRITE_DONE_CALL;
    typedef START_CALL WRITE_CALL;
    typedef std::function<void(::grpc::Status)> FINISH_CALL;
    typedef std::function<void(::grpc::Status)> CLOSE_CALL;
    typedef std::function<void(Response&)> ONREAD_CALL;
    typedef std::function<void(bool, Response&)> READ_CALL;

    struct CallInfo : public IClientData {
        CallInfo(ClientStreamReaderWriter<Request, Response>* f, int s)
                : IClientData(s, false) {
            father = f;
        }

        void Proceed(bool shutdown, bool ok) override {
            this->father->Proceed(shutdown, ok, this);
        }

        uint32_t  Status() {
            return (uint32_t)status_;
        }
        ClientStreamReaderWriter<Request, Response>* father = 0;
    };

    ClientStreamReaderWriter(Context ctx, Responder responder)
        : ClientDataCounter<IClientData>()
        , ctx_(ctx)
        , responder_(std::move(responder))
        , connect_(this, IClientData::CONNECT)
        , read_(this, IClientData::READING)
        , write_(this, IClientData::WRITING)
        , wdone_(this, IClientData::WRITESDONE)
        , finish_(this, IClientData::FINISH) {
    }

    ~ClientStreamReaderWriter() {
        for (auto req : req_list_) {
            delete req;
        }
    }

    void Proceed(bool shutdown, bool ok, CallInfo* info) {
        if (info->Status() == IClientData::CONNECT) {
            this->startcall_(ok);
            return;
        }
        if (info->Status() == IClientData::READING) {
            if (this->readcall_) {
                auto rcb = this->readcall_;
                this->readcall_ = nullptr;
                rcb(ok, this->rsp_);
            }
            return;
        }
        if (info->Status() == IClientData::WRITING) {
            this->writing_flag_ = false;
            // 数据发完后再结束
            if (this->req_list_.empty() && this->closing_flag_ > 0) {
                _TryClose();
            } else {
                _TryWrite();
            }
            return;
        }
        if (info->Status() == IClientData::WRITESDONE) {
            this->closing_flag_ = 2;
            this->onclose_(this->grpc_status_);
            //_TryClose();
            return;
        }
        if (info->Status() == IClientData::FINISH) {
            this->closing_flag_ = 3;
            this->onclose_(this->grpc_status_);
            return;
        }
    }

    bool AsyncStart(START_CALL on) {
        if (this->startcall_) {
            return false;
        }

        this->startcall_ = on;
        this->responder_->StartCall(&this->connect_);
        return true;
    }

    // 不允许连续两次调用它，需等待callback回来后
    bool AsyncRead(READ_CALL on) {
        if (this->readcall_) {
            return false;
        }
        if (this->onclose_) {
            return false;
        }

        this->readcall_ = on;
        this->rsp_.Clear();
        this->responder_->Read(&this->rsp_, &this->read_);
        return true;
    }

    // 写数据
    // 大于0表示成功发到缓存
    // 等于0表示发到缓存失败
    // 小于0表示链接被关闭了
    int Write(const Request& req) {
        if (this->closing_flag_ > 0) {
            return -1;
        }

        if (req_count_ >= 20000) {
            return 0;
        }

        req_count_ += 1;
        auto nreq = new Request;
        nreq->CopyFrom(req);
        req_list_.push_back(nreq);

        _TryWrite();
        return 1;
    }

    // 此异步cb被回调的时候才可以认为close结束，才能释放掉本对象
    bool AsyncClose(CLOSE_CALL on) {
        if (this->onclose_) {
            return false;
        }

        this->onclose_ = on;
        this->closing_flag_ = 1;
        _TryClose();
        return true;
    }

    bool OnceStart() const {
        return this->startcall_ != nullptr;
    }

    bool OnceClose() const {
        return this->onclose_ != nullptr;
    }
protected:
    void _TryWrite() {
        if (this->writing_flag_) {
            return;
        }

        if (req_list_.empty()) {
            return;
        }

        auto req = req_list_.front();
        req_list_.pop_front();
        req_count_ -= 1;

        this->writing_flag_ = true;
        this->responder_->Write(*req, &this->write_);
        delete req;
    }

    void _TryClose() {
        if (this->writing_flag_) {
            return;
        }

        if (this->closing_flag_ == 1) {
            this->responder_->WritesDone(&this->wdone_);
        } else if (this->closing_flag_ == 2) {
            // 必须是读完了消息finish才会回调
            this->responder_->Finish(&this->grpc_status_, &this->finish_);
        }
    }

protected:
    Context   ctx_;
    Responder responder_;
    CallInfo  connect_;
    CallInfo  read_;
    CallInfo  write_;
    CallInfo  wdone_;
    CallInfo  finish_;

    START_CALL startcall_;
    READ_CALL   readcall_;
    CLOSE_CALL  onclose_;

    ::grpc::Status grpc_status_;
    Response rsp_;
    std::list<Request*> req_list_;
    int req_count_ = 0;

    bool writing_flag_ = false;
    int  closing_flag_ = 0; // 0表示未被, 1表示打算writedone, 2表示打算finish, 3表示结束
};

template<class Request, class Response>
class CoClientStreamReaderWriter {
public:
    typedef typename ClientStreamReaderWriter<Request, Response>::Responder Responder;
    typedef typename ClientStreamReaderWriter<Request, Response>::Context Context;
    typedef typename ClientStreamReaderWriter<Request, Response>::ONREAD_CALL ONREAD_CALL;

    struct helper {
        helper() {}

        bool operator()(CoClientStreamReaderWriter* writer) {
            return writer->Start();
        }
    };

protected:
    ClientStreamReaderWriter<Request, Response> rw_;
    std::mutex mu_;

public:
    CoClientStreamReaderWriter(Context ctx, Responder responder)
        : rw_(ctx, std::move(responder)) {
    }

    // 是协程安全的接口，此返回值为false不能代表链接是否断开
    // 写数据
    // 大于0表示成功发到缓存
    // 等于0表示发到缓存失败
    // 小于0表示链接被关闭了
    int Write(const Request& req) {
        std::unique_lock<std::mutex> lock(mu_);
        // 以下代码调用不能引起协程挂起
        return rw_.Write(req);
    }

    // 非协程安全接口，同时只能有一个协程在调用
    // 读失败表示链接断开
    bool Read(Response* rsp) {
        if (rw_.OnceClose()) {
            return false;
        }

        co_grpc::CoWaiter waiter;
        auto cb_ok = std::make_shared<bool>();
        auto cb_rsp = std::make_shared<Response>();

        waiter.wait([this, waiter, cb_ok, cb_rsp] {
            mu_.lock();
            auto ret = rw_.AsyncRead([waiter, cb_ok, cb_rsp](bool ok, Response& rsp) {
                *cb_ok = ok;
                cb_rsp->Swap(&rsp);
                waiter.Resume();
            });
            if (!ret) {
                mu_.unlock();
                waiter.Resume();
            } else {
                mu_.unlock();
            }
        });

        rsp->Swap(cb_rsp.get());
        return *cb_ok;
    }

    // 非协程安全接口，同时只能有一个协程在调用
    // 不能与read同时发起
    ::grpc::Status Close() {
        if (rw_.OnceClose()) {
            return ::grpc::Status::CANCELLED;
        }

        co_grpc::CoWaiter waiter;
        auto cb_status = std::make_shared<::grpc::Status>();

        waiter.wait([this, waiter, cb_status] {
            mu_.lock();
            auto ret = rw_.AsyncClose([waiter, cb_status](::grpc::Status s) {
                *cb_status = s;
                waiter.Resume();
            });
            if (!ret) {
                mu_.unlock();
                waiter.Resume();
            } else {
                mu_.unlock();
            }
        });

        return *cb_status;
    }

protected:
    // 会协程阻塞直到返回
    bool Start() {
        if (rw_.OnceStart()) {
            return false;
        }

        co_grpc::CoWaiter waiter;
        auto cb_ok = std::make_shared<bool>();

        waiter.wait([this, waiter, cb_ok] {
            rw_.AsyncStart([waiter, cb_ok](bool ok) {
                *cb_ok = ok;
                waiter.Resume();
            });
        });

        return *cb_ok;
    }
};

/////////////////////////////////////////////////////////////////

inline uint64_t GetClientDataConstructCount() {
    return ClientDataCounter<IClientData>::construct_count;
}

inline uint64_t GetClientDataDestructCount() {
    return ClientDataCounter<IClientData>::destruct_count;
}

// 统计
inline void ClientStatistics() {
#ifdef Debug
        static time_t last_time = time(0);
        time_t now = time(0);
        if (now - last_time >= 60) {
            last_time = now;
            log("client construct count: %d", GetClientDataConstructCount());
            log("client destruct  count: %d", GetClientDataDestructCount());
        }
#endif
}

/////////////////////////////////////////////////////////////////

}
