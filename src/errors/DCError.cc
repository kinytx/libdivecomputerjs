#include "DCError.h"
#include <stdexcept>

const char *translate_dc_status(dc_status_t status)
{
    switch (status)
    {
    case DC_STATUS_SUCCESS:
        return "SUCCESS";
    case DC_STATUS_DONE:
        return "DONE";
    case DC_STATUS_INVALIDARGS:
        return "INVALID ARGUMENTS";
    case DC_STATUS_IO:
        return "IO ERROR";
    case DC_STATUS_CANCELLED:
        return "CANCELLED";
    case DC_STATUS_DATAFORMAT:
        return "DATAFORMAT ERROR";
    case DC_STATUS_NOACCESS:
        return "NO ACCESS";
    case DC_STATUS_NODEVICE:
        return "NO DEVICE";
    case DC_STATUS_NOMEMORY:
        return "NO MEMORY";
    case DC_STATUS_PROTOCOL:
        return "PROTOCOL ERROR";
    case DC_STATUS_TIMEOUT:
        return "TIMEOUT";
    case DC_STATUS_UNSUPPORTED:
        return "UNSUPPORTED";
    default:
        return "UNKNOWN";
    }
}

void DCError::Assert(Napi::Env env, dc_status_t expected, dc_status_t actual)
{
    if (expected != actual)
    {
        throw DCError::New(env, actual);
    }
}

void DCError::Assert(dc_status_t expected, dc_status_t actual)
{
    if (expected != actual)
    {
        throw DCError::exception(actual);
    }
}

void DCError::AssertSuccess(Napi::Env env, dc_status_t actual)
{
    DCError::Assert(env, DC_STATUS_SUCCESS, actual);
}

void DCError::AssertSuccess(dc_status_t actual)
{
    DCError::Assert(DC_STATUS_SUCCESS, actual);
}

Napi::Error DCError::New(Napi::Env env, dc_status_t status)
{
    char str[128];
    snprintf(str, sizeof(str), "Received invalid status code from libdivecomputer '%s' ", translate_dc_status(status));
    return Napi::Error::New(env, str);
}

std::logic_error DCError::exception(dc_status_t status)
{
    char str[128];
    snprintf(str, sizeof(str), "Received invalid status code from libdivecomputer '%s' ", translate_dc_status(status));
    return std::logic_error(str);
}
