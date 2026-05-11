#include "custom.h"
#include "../errors/DCError.h"
#include "../iostream.h"
#include <chrono>

Napi::FunctionReference CustomTransport::constructor;

// ── Static callbacks forwarded to instance methods ───────────────────────

static dc_status_t cb_set_timeout(void *userdata, int timeout)
{
    return static_cast<CustomTransport *>(userdata)->onSetTimeout(timeout);
}

static dc_status_t cb_set_break(void *, unsigned int) { return DC_STATUS_SUCCESS; }
static dc_status_t cb_set_dtr(void *, unsigned int) { return DC_STATUS_SUCCESS; }
static dc_status_t cb_set_rts(void *, unsigned int) { return DC_STATUS_SUCCESS; }
static dc_status_t cb_get_lines(void *, unsigned int *value)
{
    *value = 0;
    return DC_STATUS_SUCCESS;
}

static dc_status_t cb_get_available(void *userdata, size_t *value)
{
    auto t = static_cast<CustomTransport *>(userdata);
    // Not strictly needed but helpful
    *value = 0;
    return DC_STATUS_SUCCESS;
}

static dc_status_t cb_configure(void *, unsigned int, unsigned int, dc_parity_t, dc_stopbits_t, dc_flowcontrol_t)
{
    return DC_STATUS_SUCCESS;
}

static dc_status_t cb_poll(void *userdata, int timeout)
{
    return static_cast<CustomTransport *>(userdata)->onPoll(timeout);
}

static dc_status_t cb_read(void *userdata, void *data, size_t size, size_t *actual)
{
    return static_cast<CustomTransport *>(userdata)->onRead(data, size, actual);
}

static dc_status_t cb_write(void *userdata, const void *data, size_t size, size_t *actual)
{
    return static_cast<CustomTransport *>(userdata)->onWrite(data, size, actual);
}

static dc_status_t cb_ioctl(void *, unsigned int, void *, size_t)
{
    return DC_STATUS_UNSUPPORTED;
}

static dc_status_t cb_flush(void *) { return DC_STATUS_SUCCESS; }

static dc_status_t cb_purge(void *userdata, dc_direction_t)
{
    return static_cast<CustomTransport *>(userdata)->onPurge();
}

static dc_status_t cb_sleep(void *, unsigned int milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    return DC_STATUS_SUCCESS;
}

static dc_status_t cb_close(void *userdata)
{
    return static_cast<CustomTransport *>(userdata)->onClose();
}

static const dc_custom_cbs_t custom_cbs = {
    cb_set_timeout,
    cb_set_break,
    cb_set_dtr,
    cb_set_rts,
    cb_get_lines,
    cb_get_available,
    cb_configure,
    cb_poll,
    cb_read,
    cb_write,
    cb_ioctl,
    cb_flush,
    cb_purge,
    cb_sleep,
    cb_close,
};

// ── Napi class setup ────────────────────────────────────────────────────

void CustomTransport::Init(Napi::Env env, Napi::Object exports)
{
    Napi::Function func = DefineClass(
        env,
        "CustomTransport",
        {
            InstanceMethod<&CustomTransport::open>("open"),
            InstanceMethod<&CustomTransport::feedRead>("feedRead"),
            InstanceMethod<&CustomTransport::ackWrite>("ackWrite"),
            InstanceMethod<&CustomTransport::close>("close"),
        });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

    exports.Set("CustomTransport", func);
}

CustomTransport::CustomTransport(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<CustomTransport>(info)
{
    auto env = info.Env();

    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsFunction())
    {
        throw Napi::TypeError::New(env,
            "CustomTransport(transportType: number, onWrite: (buffer: Buffer) => void)");
    }

    transportType = static_cast<dc_transport_t>(info[0].As<Napi::Number>().Int32Value());

    tsfWrite = Napi::ThreadSafeFunction::New(
        env,
        info[1].As<Napi::Function>(),
        "CustomTransport write callback",
        0, // unlimited queue
        1  // initial thread count
    );
    tsfWriteReady = true;
}

CustomTransport::~CustomTransport()
{
    if (tsfWriteReady)
    {
        tsfWrite.Release();
        tsfWriteReady = false;
    }
}

// ── open() → IOStream ───────────────────────────────────────────────────

Napi::Value CustomTransport::open(const Napi::CallbackInfo &info)
{
    if (info.Length() != 1 || !info[0].IsObject())
    {
        throw Napi::TypeError::New(info.Env(), "Expected {Context} argument.");
    }

    auto context = Napi::ObjectWrap<Context>::Unwrap(info[0].As<Napi::Object>());

    dc_iostream_t *iostream;
    auto status = getNative(&iostream, context->getNative());
    DCError::AssertSuccess(info.Env(), status);

    return IOStream::constructor.New(
        {Napi::External<dc_iostream_t>::New(info.Env(), iostream)});
}

dc_status_t CustomTransport::getNative(dc_iostream_t **iostream, dc_context_t *ctx)
{
    return dc_custom_open(iostream, ctx, transportType, &custom_cbs, this);
}

// ── feedRead(buffer) — JS thread pushes data from BLE ───────────────────

Napi::Value CustomTransport::feedRead(const Napi::CallbackInfo &info)
{
    if (info.Length() != 1 || !info[0].IsBuffer())
    {
        throw Napi::TypeError::New(info.Env(), "Expected Buffer argument.");
    }

    auto buf = info[0].As<Napi::Buffer<uint8_t>>();
    {
        std::lock_guard<std::mutex> lock(mtx);
        readBuf.insert(readBuf.end(), buf.Data(), buf.Data() + buf.Length());
    }
    readCv.notify_one();

    return info.Env().Undefined();
}

// ── ackWrite() — JS thread signals write was forwarded to BLE ───────────

Napi::Value CustomTransport::ackWrite(const Napi::CallbackInfo &info)
{
    {
        std::lock_guard<std::mutex> lock(writeMtx);
        writeAcked = true;
    }
    writeCv.notify_one();
    return info.Env().Undefined();
}

// ── close() — JS thread terminates session ──────────────────────────────

Napi::Value CustomTransport::close(const Napi::CallbackInfo &info)
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        closed = true;
    }
    readCv.notify_all();
    writeCv.notify_all();

    if (tsfWriteReady)
    {
        tsfWrite.Release();
        tsfWriteReady = false;
    }

    return info.Env().Undefined();
}

// ── Worker-thread callbacks ─────────────────────────────────────────────

dc_status_t CustomTransport::onSetTimeout(int timeout)
{
    timeoutMs = timeout;
    return DC_STATUS_SUCCESS;
}

dc_status_t CustomTransport::onPoll(int timeout)
{
    std::unique_lock<std::mutex> lock(mtx);
    if (closed) return DC_STATUS_IO;
    if (!readBuf.empty()) return DC_STATUS_SUCCESS;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    readCv.wait_until(lock, deadline, [this] { return !readBuf.empty() || closed; });

    if (closed) return DC_STATUS_IO;
    return readBuf.empty() ? DC_STATUS_TIMEOUT : DC_STATUS_SUCCESS;
}

dc_status_t CustomTransport::onRead(void *data, size_t size, size_t *actual)
{
    std::unique_lock<std::mutex> lock(mtx);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    // Wait until we have at least 1 byte or closed
    readCv.wait_until(lock, deadline, [this] { return !readBuf.empty() || closed; });

    if (closed) return DC_STATUS_IO;
    if (readBuf.empty()) return DC_STATUS_TIMEOUT;

    // Copy up to `size` bytes
    size_t avail = readBuf.size();
    size_t n = (avail < size) ? avail : size;
    memcpy(data, readBuf.data(), n);
    readBuf.erase(readBuf.begin(), readBuf.begin() + n);
    *actual = n;

    return DC_STATUS_SUCCESS;
}

dc_status_t CustomTransport::onWrite(const void *data, size_t size, size_t *actual)
{
    if (closed || !tsfWriteReady) return DC_STATUS_IO;

    // Prepare write-ack state
    {
        std::lock_guard<std::mutex> lock(writeMtx);
        writeAcked = false;
    }

    // Copy data and fire to JS thread
    auto copy = new std::vector<uint8_t>(
        static_cast<const uint8_t *>(data),
        static_cast<const uint8_t *>(data) + size);

    tsfWrite.BlockingCall(copy, [](Napi::Env env, Napi::Function jsCallback, std::vector<uint8_t> *buf) {
        auto buffer = Napi::Buffer<uint8_t>::Copy(env, buf->data(), buf->size());
        jsCallback.Call({buffer});
        delete buf;
    });

    // Wait for ack from JS
    {
        std::unique_lock<std::mutex> lock(writeMtx);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        writeCv.wait_until(lock, deadline, [this] { return writeAcked || closed; });
    }

    if (closed) return DC_STATUS_IO;
    if (!writeAcked) return DC_STATUS_TIMEOUT;

    *actual = size;
    return DC_STATUS_SUCCESS;
}

dc_status_t CustomTransport::onPurge()
{
    std::lock_guard<std::mutex> lock(mtx);
    readBuf.clear();
    return DC_STATUS_SUCCESS;
}

dc_status_t CustomTransport::onClose()
{
    std::lock_guard<std::mutex> lock(mtx);
    closed = true;
    readCv.notify_all();
    writeCv.notify_all();
    return DC_STATUS_SUCCESS;
}
