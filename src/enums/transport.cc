#include "transport.h"

Napi::Object getAllTransports(Napi::Env env)
{
    auto allTransports = Napi::Object::New(env);

    auto values = ALL_TRANSPORTS;
    for (auto value : values)
    {
        allTransports.Set(value, value);
    }

    return allTransports;
}

Napi::Array translateTransports(Napi::Env env, unsigned int transports)
{
    auto array = Napi::Array::New(env);
    int iX = 0;

    if (transports & DC_TRANSPORT_USBHID)
    {
        array.Set(iX++, TRANSPORT_USBHID);
    }

    if (transports & DC_TRANSPORT_USB)
    {
        array.Set(iX++, TRANSPORT_USB);
    }

    if (transports & DC_TRANSPORT_SERIAL)
    {
        array.Set(iX++, TRANSPORT_SERIAL);
    }

    if (transports & DC_TRANSPORT_IRDA)
    {
        array.Set(iX++, TRANSPORT_IRDA);
    }

    if (transports & DC_TRANSPORT_BLE)
    {
        array.Set(iX++, TRANSPORT_BLE);
    }

    if (transports & DC_TRANSPORT_BLUETOOTH)
    {
        array.Set(iX++, TRANSPORT_BLUETOOTH);
    }

    return array;
}
