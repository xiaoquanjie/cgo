//
// Created by xiaoqj on 2024/3/1.
//

#pragma once

#include <grpc/support/log.h>
#include <functional>
#include "../idata.h"
#include "../runner/runner.h"

namespace cogrpc {
    // 一元-->结束动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ServerUnaryFinishData : public ICallData {
        Responder& responder_;
        ResponseType& response_;
        grpc::Status& status_;
        CoWaiter waiter_;

        ServerUnaryFinishData(Responder& responder, ResponseType& response, grpc::Status& status)
            : responder_(responder), response_(response), status_(status) {}

        ~ServerUnaryFinishData() override = default;

        void doRequest() override {
            this->responder_.Finish(this->response_, this->status_, this);

            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->waiter_.resume();
        }
    };

    // 一元.
    template<typename RequestType, typename ResponseType, typename Responder, typename Method, typename ObjMethod, typename Obj>
    struct ServerUnaryData : public ICallData {
        ::grpc::ServerCompletionQueue* cq_;
        ::grpc::ServerContext ctx_;
        Responder responder_;
        Method method_;
        ObjMethod cb_;
        Obj* obj_;

        RequestType request_;
        ResponseType response_;

        ServerUnaryData(::grpc::ServerCompletionQueue *cq, Method method, ObjMethod cb, Obj* obj)
            : cq_(cq), responder_(&ctx_), method_(method), cb_(cb), obj_(obj) {
        }

        ~ServerUnaryData() override = default;

        void doRequest() override {
            method_(&this->ctx_, &this->request_, &this->responder_, this->cq_, this->cq_, this);
        }

        void doResponse(bool shutdown, bool ok) override {
            if (!shutdown) {
                // 新起一个.
                auto newdata = new ServerUnaryData<RequestType, ResponseType, Responder, Method, ObjMethod, Obj>
                        (this->cq_, this->method_, this->cb_, obj_);
                newdata->doRequest();
            }

            GoRun [this, ok] {
                ServerMiddles::DoBefore(&this->ctx_, "", "");
                PointScoped This(this);
                if (ok) {
                    auto status = (This->obj_->*This->cb_)(&This->ctx_, &This->request_, &This->response_);

                    // 回复.
                    PointScoped finish(new ServerUnaryFinishData<RequestType, ResponseType, Responder>(This->responder_, This->response_, status));
                    finish->doRequest();
                }
                ServerMiddles::DoAfter(&this->ctx_, "", "");
            };
        }
    };

    ///////////////////////////////////////////////////////////////////

    // 客户端流->读动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ServerCSReadData : public ICallData {
        Responder& responder_;
        RequestType request_;
        bool ok_ = false;
        CoWaiter waiter_;

        explicit ServerCSReadData(Responder& responder) : responder_(responder) {}

        ~ServerCSReadData() override = default;

        void doRequest() override {
            this->responder_.Read(&this->request_, this);
            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->ok_ = ok;
            this->waiter_.resume();
        }
    };

    // 客户端流->读者.
    template<typename RequestType, typename ResponseType, typename Responder>
    class ServerStreamReader {
        Responder& responder_;
        CoMutex mu_;

    public:
        explicit ServerStreamReader(Responder& responder) : responder_(responder) {}

        // 协程安全.
        bool Read(RequestType* req) {
            CoScopedLock sl(this->mu_);

            PointScoped data(new ServerCSReadData<RequestType, ResponseType, Responder>(this->responder_));
            data->doRequest();
            if (data->ok_) {
                req->Swap(&data->request_);
            }
            return data->ok_;
        }
    };

    // 客户端流->结束动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ServerCSFinishData : public ICallData {
        Responder& responder_;
        ResponseType& response_;
        grpc::Status& status_;
        CoWaiter waiter_;

        ServerCSFinishData(Responder& responder, ResponseType& response, grpc::Status& status)
            : responder_(responder), response_(response), status_(status) {}

        ~ServerCSFinishData() override = default;

        void doRequest() override {
            this->responder_.Finish(this->response_, this->status_, this);
            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->waiter_.resume();
        }
    };

    // 客户端流.
    template<typename RequestType, typename ResponseType, typename Responder, typename Method, typename ObjMethod, typename Obj>
    struct ServerCSData : public ICallData {
        ::grpc::ServerCompletionQueue* cq_;
        ::grpc::ServerContext ctx_;
        Responder responder_;
        Method method_;
        ObjMethod cb_;
        Obj* obj_;

        ServerCSData(::grpc::ServerCompletionQueue *cq, Method method, ObjMethod cb, Obj* obj)
            : cq_(cq), responder_(&ctx_), method_(method), cb_(cb), obj_(obj) {
        }

        ~ServerCSData() override = default;

        void doRequest() override {
            method_(&this->ctx_, &this->responder_, this->cq_, this->cq_, this);
        }

        void doResponse(bool shutdown, bool ok) override {
            if (!shutdown) {
                // 新起一个.
                auto newdata = new ServerCSData<RequestType, ResponseType, Responder, Method, ObjMethod, Obj>
                        (this->cq_, this->method_, this->cb_, this->obj_);
                newdata->doRequest();
            }

            GoRun [this, ok] {
                ServerMiddles::DoBefore(&this->ctx_, "", "");
                PointScoped This(this);
                if (ok) {
                    ResponseType response;
                    // 起一个reader.
                    ServerStreamReader<RequestType, ResponseType, Responder> reader(This->responder_);
                    auto status = (This->obj_->*This->cb_)(&This->ctx_, &reader, &response);

                    PointScoped finish(new ServerCSFinishData<RequestType, ResponseType, Responder>(This->responder_, response, status));
                    finish->doRequest();
                }
                ServerMiddles::DoAfter(&This->ctx_, "", "");
            };
        }
    };

    ///////////////////////////////////////////////////////////////////

    // 服务端流->写动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ServerSSWriteData : public ICallData {
        Responder& responder_;
        const ResponseType& response_;
        bool ok_ = false;
        CoWaiter waiter_;

        ServerSSWriteData(Responder& responder, const ResponseType& response)
            : responder_(responder), response_(response) {}

        ~ServerSSWriteData() override = default;

        void doRequest() override {
            this->responder_.Write(this->response_, this);
            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->ok_ = ok;
            this->waiter_.resume();
        }
    };

    // 服务端流->写者.
    template<typename RequestType, typename ResponseType, typename Responder>
    class ServerStreamWriter {
        Responder& responder_;
        CoMutex mu_;

    public:
        explicit ServerStreamWriter(Responder& responder) : responder_(responder) {}

        // 协程安全.
        bool Write(const ResponseType& rsp) {
            CoScopedLock sl(this->mu_);

            PointScoped data(new ServerSSWriteData<RequestType, ResponseType, Responder>(this->responder_, rsp));
            data->doRequest();
            return data->ok_;
        }
    };

    // 服务端流->结束动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ServerSSFinishData : public ICallData {
        Responder& responder_;
        grpc::Status& status_;
        CoWaiter waiter_;

        ServerSSFinishData(Responder& responder, grpc::Status& status)
                : responder_(responder), status_(status) {}

        ~ServerSSFinishData() override = default;

        void doRequest() override {
            this->responder_.Finish(this->status_, this);
            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->waiter_.resume();
        }
    };

    // 服务器流.
    template<typename RequestType, typename ResponseType, typename Responder, typename Method, typename ObjMethod, typename Obj>
    struct ServerSSData : public ICallData {
        ::grpc::ServerCompletionQueue* cq_;
        ::grpc::ServerContext ctx_;
        Responder responder_;
        RequestType request_;
        Method method_;
        ObjMethod cb_;
        Obj* obj_;

        ServerSSData(::grpc::ServerCompletionQueue *cq, Method method, ObjMethod cb, Obj* obj)
            : cq_(cq), responder_(&ctx_), method_(method), cb_(cb), obj_(obj) {
        }

        ~ServerSSData() override = default;

        void doRequest() override {
            method_(&this->ctx_, &this->request_, &this->responder_, this->cq_, this->cq_, this);
        }

        void doResponse(bool shutdown, bool ok) override {
            if (!shutdown) {
                // 新起一个.
                auto newdata = new ServerSSData<RequestType, ResponseType, Responder, Method, ObjMethod, Obj>
                        (this->cq_, this->method_, this->cb_, this->obj_);
                newdata->doRequest();
            }

            GoRun [this, ok] {
                ServerMiddles::DoBefore(&this->ctx_, "", "");
                PointScoped This(this);
                if (ok) {
                    // 起一个writer.
                    ServerStreamWriter<RequestType, ResponseType, Responder> writer(this->responder_);
                    auto status = (this->obj_->*this->cb_)(&this->ctx_, &this->request_, &writer);

                    PointScoped finish(new ServerSSFinishData<RequestType, ResponseType, Responder>(this->responder_, status));
                    finish->doRequest();
                }
                ServerMiddles::DoAfter(&this->ctx_, "", "");
            };
        }
    };

    /////////////////////////////////////////////////////////////

    // 服务器双流--读动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ServerDSReadData : public ICallData {
        Responder& responder_;
        RequestType request_;
        bool ok_ = false;
        CoWaiter waiter_;

        explicit ServerDSReadData(Responder& responder) : responder_(responder) {}

        void doRequest() override {
            this->responder_.Read(&this->request_, this);
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->ok_ = ok;
            this->waiter_.resume();
        }
    };

    // 服务器双流--写动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ServerDSWriteData : public ICallData {
        Responder& responder_;
        const ResponseType& response_;
        bool ok_ = false;
        CoWaiter waiter_;

        ServerDSWriteData(Responder& responder, const ResponseType& response)
            : responder_(responder), response_(response) {}

        void doRequest() override {
            this->responder_.Write(this->response_, this);
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->ok_ = ok;
            this->waiter_.resume();
        }
    };

    // 服务器双流--读写者.
    template<typename RequestType, typename ResponseType, typename Responder>
    class ServerStreamReaderWriter {
        Responder& responder_;
        CoMutex rmu_;
        CoMutex wmu_;

    public:
        explicit ServerStreamReaderWriter(Responder& responder) : responder_(responder) {}

        // 协程安全.
        bool Read(RequestType* req) {
            CoScopedLock sl(this->rmu_);

            PointScoped data(new ServerDSReadData<RequestType, ResponseType, Responder>(this->responder_));
            data->doRequest();
            if (data->ok_) {
                req->Swap(&data->request_);
            }
            return data->ok_;
        }

        // 协程安全.
        bool Write(const ResponseType& rsp) {
            CoScopedLock sl(this->wmu_);

            PointScoped data(new ServerDSWriteData<RequestType, ResponseType, Responder>(this->responder_, rsp));
            data->doRequest();
            return data->ok_;
        }
    };

    // 服务器双流--结束动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ServerDSFinishData : public ICallData {
        Responder& responder_;
        grpc::Status& status_;
        CoWaiter waiter_;

        ServerDSFinishData(Responder& responder, grpc::Status& status)
            : responder_(responder), status_(status) {}

        ~ServerDSFinishData() override = default;

        void doRequest() override {
            this->responder_.Finish(this->status_, this);
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->waiter_.resume();
        }
    };

    // 服务器双流.
    template<typename RequestType, typename ResponseType, typename Responder, typename Method, typename ObjMethod, typename Obj>
    struct ServerDSData : public ICallData {
        ::grpc::ServerCompletionQueue* cq_;
        ::grpc::ServerContext ctx_;
        Responder responder_;
        Method method_;
        ObjMethod cb_;
        Obj* obj_;

        ServerDSData(::grpc::ServerCompletionQueue *cq, Method method, ObjMethod cb, Obj* obj)
            : cq_(cq), responder_(&ctx_), method_(method), cb_(cb), obj_(obj) {
        }

        ~ServerDSData() override = default;

        void doRequest() override {
            method_(&this->ctx_, &this->responder_, this->cq_, this->cq_, this);
        }

        void doResponse(bool shutdown, bool ok) override {
            if (!shutdown) {
                // 新起一个.
                auto newdata = new ServerDSData<RequestType, ResponseType, Responder, Method, ObjMethod, Obj>
                        (this->cq_, this->method_, this->cb_, this->obj_);
                newdata->doRequest();
            }

            GoRun [this, ok] {
                ServerMiddles::DoBefore(&this->ctx_, "", "");
                PointScoped This(this);
                if (ok) {
                    // 起一个readwrite.
                    ServerStreamReaderWriter<RequestType, ResponseType, Responder> rw(this->responder_);
                    auto status = (this->obj_->*this->cb_)(&this->ctx_, &rw);

                    PointScoped finish(new ServerDSFinishData<RequestType, ResponseType, Responder>(this->responder_, status));
                    finish->doRequest();
                }
                ServerMiddles::DoAfter(&this->ctx_, "", "");
            };
        }
    };
}
