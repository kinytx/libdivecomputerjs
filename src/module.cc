#include <napi.h>
#include "version.h"
#include "descriptor.h"
#include "context.h"
#include "device.h"
#include "enums/loglevel.h"
#include "enums/transport.h"
#include "enums/eventtypes.h"
#include "enums/fieldtypes.h"
#include "enums/fieldvalues/divemode.h"
#include "enums/fieldvalues/tankvolume.h"
#include "enums/fieldvalues/watertype.h"
#include "enums/sampletypes.h"
#include "enums/samplevalues/eventsampletype.h"
#include "transports/usbhid.h"
#include "transports/serial.h"
#include "transports/irda.h"
#include "transports/bluetooth.h"
#include "transports/custom.h"
#include "iostream.h"
#include "parser.h"
#include "asyncdevicereader.h"
#include "asyncdevicereaderworker.h"

using namespace Napi;

Napi::Object Init(Napi::Env env, Napi::Object exports)
{
    Descriptor::Init(env, exports);
    Context::Init(env, exports);
    USBHIDTransport::Init(env, exports);
    BluetoothTransport::Init(env, exports);
    IRDATransport::Init(env, exports);
    SerialTransport::Init(env, exports);
    CustomTransport::Init(env, exports);
    Device::Init(env, exports);
    Parser::Init(env, exports);
    IOStream::Init(env, exports);
    AsyncDeviceReader::Init(env, exports);

    exports.Set(Napi::String::New(env, "version"), Napi::Function::New(env, getVersion));
    exports.Set(Napi::String::New(env, "LogLevel"), getLogLevels(env));
    exports.Set(Napi::String::New(env, "EventType"), getAllEventTypes(env));
    exports.Set(Napi::String::New(env, "Transport"), getAllTransports(env));
    exports.Set(Napi::String::New(env, "FieldType"), getAllFieldTypes(env));
    exports.Set(Napi::String::New(env, "DiveMode"), getAllDiveModes(env));
    exports.Set(Napi::String::New(env, "TankVolume"), getAllTankVolumes(env));
    exports.Set(Napi::String::New(env, "WaterType"), getAllWaterTypes(env));
    exports.Set(Napi::String::New(env, "SampleEventType"), getAllSampleEventTypes(env));
    exports.Set(Napi::String::New(env, "SampleType"), getAllSampleTypes(env));

    return exports;
}

NODE_API_MODULE(addon, Init)
