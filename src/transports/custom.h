#pragma once
#include <napi.h>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <libdivecomputer/custom.h>
#include "../context.h"
#include "../descriptor.h"
#include "nativetransport.h"

class CustomTransport : public NativeTransport, public Napi::ObjectWrap<CustomTransport>
{
public:
    static void Init(Napi::Env env, Napi::Object exports);
    static Napi::FunctionReference constructor;

    CustomTransport(const Napi::CallbackInfo &);
    ~CustomTransport();

    Napi::Value open(const Napi::CallbackInfo &);
    Napi::Value feedRead(const Napi::CallbackInfo &);
    Napi::Value ackWrite(const Napi::CallbackInfo &);
    Napi::Value close(const Napi::CallbackInfo &);

    dc_status_t getNative(dc_iostream_t **iostream, dc_context_t *ctx);

    // Custom IOStream callbacks (called from worker thread)
    dc_status_t onRead(void *data, size_t size, size_t *actual);
    dc_status_t onWrite(const void *data, size_t size, size_t *actual);
    dc_status_t onSetTimeout(int timeout);
    dc_status_t onPoll(int timeout);
    dc_status_t onPurge();
    dc_status_t onClose();

private:
    dc_transport_t transportType;
    int timeoutMs = 10000;
    bool closed = false;

    std::mutex mtx;
    std::condition_variable readCv;
    std::vector<uint8_t> readBuf;

    std::mutex writeMtx;
    std::condition_variable writeCv;
    bool writeAcked = false;

    Napi::ThreadSafeFunction tsfWrite;
    bool tsfWriteReady = false;
};
