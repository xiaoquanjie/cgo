//
// Created by xiaoqj on 2024/3/1.
//

#pragma once

#include <grpc/support/log.h>
#include <functional>
#include "../idata.h"
#include "../runner/runner.h"

namespace cogrpc {
    // 一元.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ClientUnaryData : public ICallData {
        Responder responder_;
        // 协议回复值.
        ResponseType response_;
        // 协议状态码.
        ::grpc::Status status_;
        CoWaiter waiter_;

        explicit ClientUnaryData(Responder responder) {
            this->responder_ = responder;
        }

        ~ClientUnaryData() override = default;

        void doRequest() override {
            // StartCall initiates the RPC call
            this->responder_->StartCall();
            // Request that, upon completion of the RPC, "reply" be updated with the
            // server's response; "status" with the indication of whether the operation
            // was successful. Tag the request with the memory address of the call
            // object.
            this->responder_->Finish(&this->response_, &this->status_, this);

            // wait
            this->waiter_.wait();
        }

        void doResponse(bool, bool) override {
            this->waiter_.resume();
        }
    };

    ////////////////////////////////////////////////////////

    // 客户端写流-->开始动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ClientCSStartData : public ICallData {
        Responder responder_;
        // 标识请求是否成功.
        bool ok_ = false;
        CoWaiter waiter_;

        explicit ClientCSStartData(Responder responder) {
            this->responder_ = responder;
        }

        ~ClientCSStartData() override = default;

        void doRequest() override {
            // StartCall initiates the RPC call
            this->responder_->StartCall(this);

            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->ok_ = ok;
            this->waiter_.resume();
        }
    };

    // 客户端写流-->写动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ClientCSWriteData : public ICallData {
        Responder responder_;
        // 协议请求的数据.
        const RequestType& request_;
        // 标识请求是否成功.
        bool ok_ = false;
        CoWaiter waiter_;

        ClientCSWriteData(Responder responder, const RequestType& request) : responder_(responder), request_(request) {
        }

        ~ClientCSWriteData() override = default;

        void doRequest() override {
            this->responder_->Write(request_, this);

            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->ok_ = ok;
            this->waiter_.resume();
        }
    };

    // 客户端写流-->写结束动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ClientCSWriteDoneData : public ICallData {
        Responder responder_;
        // 标识请求是否成功.
        bool ok_ = false;
        CoWaiter waiter_;

        explicit ClientCSWriteDoneData(Responder responder) : responder_(responder) {
        }

        ~ClientCSWriteDoneData() override = default;

        void doRequest() override {
            this->responder_->WritesDone(this);

            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->ok_ = ok;
            this->waiter_.resume();
        }
    };

    // 客户端写流-->结束动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ClientCSFinishData : public ICallData {
        Responder responder_;
        // 协议状态码.
        ::grpc::Status status_;
        CoWaiter waiter_;

        explicit ClientCSFinishData(Responder responder) : responder_(responder) {
        }

        ~ClientCSFinishData() override = default;

        void doRequest() override {
            this->responder_->Finish(&this->status_, this);

            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->waiter_.resume();
        }
    };

    // 客户端写流.
    template<typename RequestType, typename ResponseType, typename Responder>
    class ClientStreamWriter {
        typedef std::shared_ptr<::grpc::ClientContext> Context;

        ResponseType* outresponse_;
        PointScoped<ResponseType> newresponse_;
        Responder responder_;
        Context ctx_;
        CoMutex mu_;

    public:
        ClientStreamWriter(Responder responder,
                           const Context& ctx,
                           ResponseType* outresponse,
                           ResponseType* newresponse)
            : newresponse_(newresponse) {
            this->ctx_ = ctx;
            this->outresponse_ = outresponse;
            responder_ = responder;
        }

        ~ClientStreamWriter() {
            // 非常重要这一句,否则会崩溃.
            responder_.reset();
        }

        // 协程安全
        bool Write(const RequestType& req) {
            CoScopedLock sl(this->mu_);

            PointScoped ps(new ClientCSWriteData<RequestType, ResponseType, Responder>(this->responder_, req));
            ps->doRequest();
            return ps->ok_;
        }

        // 协程安全
        ::grpc::Status Finish() {
            CoScopedLock sl(this->mu_);

            PointScoped done(new ClientCSWriteDoneData<RequestType, ResponseType, Responder>(this->responder_));
            done->doRequest();
            if (!done->ok_) {
                return ::grpc::Status::CANCELLED;
            }

            PointScoped finish(new ClientCSFinishData<RequestType, ResponseType, Responder>(this->responder_));
            finish->doRequest();

            this->newresponse_->Swap(this->outresponse_);
            return finish->status_;
        }
    };

    ////////////////////////////////////////////////////////

    // 客户端读流--> 开始动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ClientSSStartData : public ICallData {
        Responder responder_;
        // 标识请求是否成功.
        bool ok_ = false;
        CoWaiter waiter_;

        explicit ClientSSStartData(Responder responder) : responder_(responder) {}

        ~ClientSSStartData() override = default;

        void doRequest() override {
            this->responder_->StartCall(this);

            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->ok_ = ok;
            this->waiter_.resume();
        }
    };

    // 客户端读流--> 读动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ClientSSReadData : public ICallData {
        Responder responder_;
        ResponseType response_;
        // 标识请求是否成功.
        bool ok_ = false;
        CoWaiter waiter_;

        explicit ClientSSReadData(Responder responder) : responder_(responder) {}

        ~ClientSSReadData() override = default;

        void doRequest() override {
            this->responder_->Read(&this->response_, this);

            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->ok_ = ok;
            this->waiter_.resume();
        }
    };

    // 客户端读流--> 结束动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ClientSSFinishData : public ICallData {
        Responder responder_;
        // 协议状态码.
        ::grpc::Status status_;
        CoWaiter waiter_;

        explicit ClientSSFinishData(Responder responder) : responder_(responder) {}

        ~ClientSSFinishData() override = default;

        void doRequest() override {
            this->responder_->Finish(&this->status_, this);

            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->waiter_.resume();
        }
    };

    // 客户端读流.
    template<typename RequestType, typename ResponseType, typename Responder>
    class ClientStreamReader {
        typedef std::shared_ptr<::grpc::ClientContext> Context;

        Responder responder_;
        Context ctx_;
        CoMutex mu_;

    public:
        ClientStreamReader(Responder responder, Context ctx)
            : responder_(responder), ctx_(std::move(ctx)) {}

        ~ClientStreamReader() {
            ResponseType rsp;
            while(Read(&rsp));
            Finish();
            // 非常重要这一句,否则会崩溃.
            responder_.reset();
        }

        // 协程安全.
        bool Read(ResponseType* rsp) {
            CoScopedLock sl(this->mu_);

            PointScoped data(new ClientSSReadData<RequestType, ResponseType, Responder>(this->responder_));
            data->doRequest();
            if (data->ok_) {
                rsp->Swap(&data->response_);
            }
            return data->ok_;
        }

    protected:
        ::grpc::Status Finish() {
            PointScoped data(new ClientSSFinishData<RequestType, ResponseType, Responder>(this->responder_));
            data->doRequest();
            return data->status_;
        }
    };

    ////////////////////////////////////////////////////////

    // 客户端双流--> 开始动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ClientDSStartData : public ICallData {
        Responder& responder_;
        // 标识请求是否成功.
        bool ok_ = false;
        CoWaiter waiter_;

        explicit ClientDSStartData(Responder responder) : responder_(responder) {}

        ~ClientDSStartData() override = default;

        void doRequest() override {
            this->responder_->StartCall(this);

            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->ok_ = ok;
            this->waiter_.resume();
        }
    };

    // 客户端双流--> 读动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ClientDSReadData : public ICallData {
        Responder responder_;
        ResponseType reponse_;
        // 标识请求是否成功.
        bool ok_ = false;
        CoWaiter waiter_;

        explicit ClientDSReadData(Responder responder) : responder_(responder) {}

        ~ClientDSReadData() override = default;

        void doRequest() override {
            this->responder_->Read(&this->reponse_, this);

            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->ok_ = ok;
            this->waiter_.resume();
        }
    };

    // 客户端双流--> 写动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ClientDSWriteData : public ICallData {
        Responder responder_;
        const RequestType& request_;
        // 标识请求是否成功.
        bool ok_ = false;
        CoWaiter waiter_;

        ClientDSWriteData(Responder responder, const RequestType& request) : responder_(responder), request_(request) {}

        ~ClientDSWriteData() override = default;

        void doRequest() override {
            this->responder_->Write(request_, this);

            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->ok_ = ok;
            this->waiter_.resume();
        }
    };

    // 客户端双流--写结束动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ClientDSWriteDoneData : public ICallData {
        Responder responder_;
        bool ok_ = false;
        CoWaiter waiter_;

        explicit ClientDSWriteDoneData(Responder responder) : responder_(responder) {}

        ~ClientDSWriteDoneData() override = default;

        void doRequest() override {
            this->responder_->WritesDone(this);

            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->ok_ = ok;
            this->waiter_.resume();
        }
    };

    // 客户端双流--> 结束动作.
    template<typename RequestType, typename ResponseType, typename Responder>
    struct ClientDSFinishData : public ICallData {
        Responder responder_;
        // 协议状态码.
        ::grpc::Status status_;
        CoWaiter waiter_;

        explicit ClientDSFinishData(Responder responder) : responder_(responder) {}

        ~ClientDSFinishData() override = default;

        void doRequest() override {
            this->responder_->Finish(&this->status_, this);

            // wait
            this->waiter_.wait();
        }

        void doResponse(bool shutdown, bool ok) override {
            this->waiter_.resume();
        }
    };

    // 客户端双流.
    template<typename RequestType, typename ResponseType, typename Responder>
    class ClientStreamReaderWriter {
        typedef std::shared_ptr<::grpc::ClientContext> Context;

        Responder responder_;
        Context ctx_;
        CoMutex rmu_;
        CoMutex wmu_;

    public:
        ClientStreamReaderWriter(Responder responder, Context ctx)
            : responder_(responder), ctx_(std::move(ctx)) {
        }

        ~ClientStreamReaderWriter() {
            Finish();
            // 非常重要这一句,否则会崩溃.
            responder_.reset();
        }

        // 协程安全.
        bool Write(const RequestType& req) {
            CoScopedLock sl(this->wmu_);

            PointScoped data(new ClientDSWriteData<RequestType, ResponseType, Responder>(this->responder_, req));
            data->doRequest();
            return data->ok_;
        }

        // 协程安全.
        bool Read(ResponseType* rsp) {
            CoScopedLock sl(this->rmu_);

            PointScoped data(new ClientDSReadData<RequestType, ResponseType, Responder>(this->responder_));
            data->doRequest();
            if (data->ok_) {
                rsp->Swap(&data->reponse_);
            }
            return data->ok_;
        }

    protected:
        // 该函数在以下情况会返回.
        // 1:read接口返回false.
        // 2:server返回非non-OK status.
        // 3:库错误.
        ::grpc::Status Finish() {
            PointScoped done(new ClientDSWriteDoneData<RequestType, ResponseType, Responder>(this->responder_));
            done->doRequest();
            if (!done->ok_) {
                return ::grpc::Status::CANCELLED;
            }

            ResponseType rsp;
            while (Read(&rsp));

            PointScoped data(new ClientDSFinishData<RequestType, ResponseType, Responder>(this->responder_));
            data->doRequest();
            return data->status_;
        }
    };
}




