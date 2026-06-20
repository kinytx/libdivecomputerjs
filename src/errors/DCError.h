#pragma once
#include <napi.h>
#include <libdivecomputer/common.h>
#include <stdexcept>

class DCError
{
public:
    static Napi::Error New(Napi::Env env, dc_status_t status);
    static void Assert(Napi::Env env, dc_status_t expected, dc_status_t actual);
    static void Assert(dc_status_t expected, dc_status_t actual);
    static void AssertSuccess(Napi::Env env, dc_status_t actual);
    static void AssertSuccess(dc_status_t actual);
    static std::logic_error exception(dc_status_t actual);
};
