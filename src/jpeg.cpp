#include <nan.h>
#include <node.h>
#include <node_buffer.h>
#include <jpeglib.h>
#include <cstdlib>
#include <cstring>

#include "common.h"
#include "jpeg.h"
#include "jpeg_encoder.h"

using namespace v8;
using namespace node;

using v8::Object;
using v8::Handle;

void
Jpeg::Initialize(Handle<Object> target)
{
    ;

    Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    Nan::SetPrototypeMethod(t, "encode", JpegEncodeAsync);
    Nan::SetPrototypeMethod(t, "encodeSync", JpegEncodeSync);
    Nan::SetPrototypeMethod(t, "setQuality", SetQuality);
    Nan::SetPrototypeMethod(t, "setSmoothing", SetSmoothing);
    Nan::Set(target, Nan::New("Jpeg").ToLocalChecked(), Nan::GetFunction(t).ToLocalChecked());
}

Jpeg::Jpeg(unsigned char *ddata, int wwidth, int hheight, buffer_type bbuf_type) :
    jpeg_encoder(ddata, wwidth, hheight, 60, bbuf_type) {}

Handle<Value>
Jpeg::JpegEncodeSync()
{
    try {
        jpeg_encoder.encode();
    }
    catch (const char *err) {
        Nan::ThrowError(err);
    }

    Handle<Object> retbuf = Nan::CopyBuffer((const char *) jpeg_encoder.get_jpeg(), jpeg_encoder.get_jpeg_len()).ToLocalChecked();
    // memcpy(Buffer::Data(retbuf), , jpeg_len);
    return retbuf;
}

void
Jpeg::SetQuality(int q)
{
    jpeg_encoder.set_quality(q);
}

void
Jpeg::SetSmoothing(int s)
{
    jpeg_encoder.set_smoothing(s);
}

NAN_METHOD(Jpeg::New)
{
    if (info.Length() < 3) {
        Nan::ThrowError("At least three arguments required - buffer, width, height, [and buffer type]");
    }
    if (!Buffer::HasInstance(info[0])) {
        Nan::ThrowError("First argument must be Buffer.");
    }
    if (!info[1]->IsInt32()) {
        Nan::ThrowError("Second argument must be integer width.");
    }
    if (!info[2]->IsInt32()) {
        Nan::ThrowError("Third argument must be integer height.");
    }

    int w = Nan::To<int32_t>(info[1]).FromJust();
    int h = Nan::To<int32_t>(info[2]).FromJust();

    if (w < 0) {
        Nan::ThrowError("Width can't be negative.");
    }
    if (h < 0) {
        Nan::ThrowError("Height can't be negative.");
    }

    buffer_type buf_type = BUF_RGB;
    if (info.Length() == 4) {
        if (!info[3]->IsString()) {
            Nan::ThrowError("Fifth argument must be a string. Either 'rgb', 'bgr', 'rgba' or 'bgra'.");
        }

        Nan::Utf8String bt(info[3]->ToString());
        if (!(str_eq(*bt, "rgb") || str_eq(*bt, "bgr") ||
            str_eq(*bt, "rgba") || str_eq(*bt, "bgra")))
        {
            Nan::ThrowError("Buffer type must be 'rgb', 'bgr', 'rgba' or 'bgra'.");
        }

        if (str_eq(*bt, "rgb")) {
            buf_type = BUF_RGB;
        } else if (str_eq(*bt, "bgr")) {
            buf_type = BUF_BGR;
        } else if (str_eq(*bt, "rgba")) {
            buf_type = BUF_RGBA;
        } else if (str_eq(*bt, "bgra")) {
            buf_type = BUF_BGRA;
        } else {
            Nan::ThrowError("Buffer type wasn't 'rgb', 'bgr', 'rgba' or 'bgra'.");
        }
    }

    Local<Object> buffer = info[0]->ToObject();
    Jpeg *jpeg = new Jpeg((unsigned char*) Buffer::Data(buffer), w, h, buf_type);
    jpeg->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
}

NAN_METHOD(Jpeg::JpegEncodeSync)
{
    Jpeg *jpeg = Nan::ObjectWrap::Unwrap<Jpeg>(info.This());
    info.GetReturnValue().Set(jpeg->JpegEncodeSync());
}

NAN_METHOD(Jpeg::SetQuality)
{
    if (info.Length() != 1) {
        Nan::ThrowError("One argument required - quality");
    }

    if (!info[0]->IsInt32()) {
        Nan::ThrowError("First argument must be integer quality");
    }

    int32_t q = Nan::To<int32_t>(info[0]).FromJust();

    if (q < 0) {
        Nan::ThrowError("Quality must be greater or equal to 0.");
    }
    if (q > 100) {
        Nan::ThrowError("Quality must be less than or equal to 100.");
    }

    Jpeg *jpeg = Nan::ObjectWrap::Unwrap<Jpeg>(info.This());
    jpeg->SetQuality(q);

    info.GetReturnValue().SetUndefined();
}

NAN_METHOD(Jpeg::SetSmoothing)
{
    ;

    if (info.Length() != 1) {
        Nan::ThrowError("One argument required - quality");
    }

    if (!info[0]->IsInt32()) {
        Nan::ThrowError("First argument must be integer quality");
    }

    int32_t s = Nan::To<int32_t>(info[0]).FromJust();

    if (s < 0) {
        Nan::ThrowError("Smoothing must be greater or equal to 0.");
    }
    if (s > 100) {
        Nan::ThrowError("Smoothing must be less than or equal to 100.");
    }

    Jpeg *jpeg = Nan::ObjectWrap::Unwrap<Jpeg>(info.This());
    jpeg->SetSmoothing(s);

    info.GetReturnValue().SetUndefined();
}

void
Jpeg::UV_JpegEncode(uv_work_t *req)
{
    encode_request *enc_req = (encode_request *)req->data;
    Jpeg *jpeg = (Jpeg *)enc_req->jpeg_obj;

    try {
        jpeg->jpeg_encoder.encode();
        enc_req->jpeg_len = jpeg->jpeg_encoder.get_jpeg_len();
        enc_req->jpeg = (char *)malloc(sizeof(*enc_req->jpeg)*enc_req->jpeg_len);
        if (!enc_req->jpeg) {
            enc_req->error = strdup("malloc in Jpeg::UV_JpegEncode failed.");
            return;
        }
        else {
            memcpy(enc_req->jpeg, jpeg->jpeg_encoder.get_jpeg(), enc_req->jpeg_len);
        }
    }
    catch (const char *err) {
        enc_req->error = strdup(err);
    }
}

void 
Jpeg::UV_JpegEncodeAfter(uv_work_t *req)
{
    ;

    encode_request *enc_req = (encode_request *)req->data;
    delete req;

    Handle<Value> argv[2];

    if (enc_req->error) {
        argv[0] = Nan::Undefined();
        argv[1] = Nan::Error(enc_req->error);
    }
    else {
        Handle<Object> buf = Nan::NewBuffer(enc_req->jpeg, enc_req->jpeg_len).ToLocalChecked();
        argv[0] = buf;
        argv[1] = Nan::Undefined();
    }

    Nan::TryCatch try_catch; // don't quite see the necessity of this

    enc_req->callback->Call(2, argv);

    if (try_catch.HasCaught())
        FatalException(try_catch);

    delete enc_req->callback;
    free(enc_req->jpeg);
    free(enc_req->error);

    ((Jpeg *)enc_req->jpeg_obj)->Unref();
    free(enc_req);
}

NAN_METHOD(Jpeg::JpegEncodeAsync)
{
    ;

    if (info.Length() != 1) {
        Nan::ThrowError("One argument required - callback function.");
    }

    if (!info[0]->IsFunction()) {
        Nan::ThrowError("First argument must be a function.");
    }

    Local<Function> callback = info[0].As<Function>();
    Jpeg *jpeg = Nan::ObjectWrap::Unwrap<Jpeg>(info.This());

    encode_request *enc_req = (encode_request *)malloc(sizeof(*enc_req));
    if (!enc_req) {
        Nan::ThrowError("malloc in Jpeg::JpegEncodeAsync failed.");
    }

    enc_req->callback = new Nan::Callback(callback);
    enc_req->jpeg_obj = jpeg;
    enc_req->jpeg = NULL;
    enc_req->jpeg_len = 0;
    enc_req->error = NULL;

    uv_work_t* req = new uv_work_t;
    req->data = enc_req;
    uv_queue_work(uv_default_loop(), req, UV_JpegEncode, (uv_after_work_cb)UV_JpegEncodeAfter);

    jpeg->Ref();

    info.GetReturnValue().SetUndefined();
}

