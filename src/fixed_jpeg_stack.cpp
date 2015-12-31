#include <nan.h>
#include <node.h>
#include <node_buffer.h>
#include <jpeglib.h>
#include <cstdlib>
#include <cstring>

#include "common.h"
#include "fixed_jpeg_stack.h"
#include "jpeg_encoder.h"

using v8::Object;
using v8::Handle;
using v8::Local;
using v8::Function;
using v8::FunctionTemplate;
using v8::String;

void
FixedJpegStack::Initialize(Handle<Object> target)
{
    Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    Nan::SetPrototypeMethod(t, "encode", JpegEncodeAsync);
    Nan::SetPrototypeMethod(t, "encodeSync", JpegEncodeSync);
    Nan::SetPrototypeMethod(t, "push", Push);
    Nan::SetPrototypeMethod(t, "setQuality", SetQuality);
    Nan::Set(target, Nan::New("FixedJpegStack").ToLocalChecked(), Nan::GetFunction(t).ToLocalChecked());
}

FixedJpegStack::FixedJpegStack(int wwidth, int hheight, buffer_type bbuf_type) :
    width(wwidth), height(hheight), quality(60), buf_type(bbuf_type)
{
    data = (unsigned char *)calloc(width*height*3, sizeof(*data));
    if (!data) {
        throw "calloc in FixedJpegStack::FixedJpegStack failed!";
    }
}

Handle<Value>
FixedJpegStack::JpegEncodeSync()
{
    JpegEncoder jpeg_encoder(data, width, height, quality, BUF_RGB);
    jpeg_encoder.encode();
    Handle<Object> retbuf = Nan::CopyBuffer((const char*) jpeg_encoder.get_jpeg(), jpeg_encoder.get_jpeg_len()).ToLocalChecked();
    return retbuf;
}

void
FixedJpegStack::Push(unsigned char *data_buf, int x, int y, int w, int h)
{
    int start = y*width*3 + x*3;

    switch (buf_type) {
    case BUF_RGB:
        for (int i = 0; i < h; i++) {
            unsigned char *datap = &data[start + i*width*3];
            for (int j = 0; j < w; j++) {
                *datap++ = *data_buf++;
                *datap++ = *data_buf++;
                *datap++ = *data_buf++;
            }
        }
        break;

    case BUF_BGR:
        for (int i = 0; i < h; i++) {
            unsigned char *datap = &data[start + i*width*3];
            for (int j = 0; j < w; j++) {
                *datap++ = *(data_buf+2);
                *datap++ = *(data_buf+1);
                *datap++ = *data_buf;
                data_buf+=3;
            }
        }
        break;

    case BUF_RGBA:
        for (int i = 0; i < h; i++) {
            unsigned char *datap = &data[start + i*width*3];
            for (int j = 0; j < w; j++) {
                *datap++ = *data_buf++;
                *datap++ = *data_buf++;
                *datap++ = *data_buf++;
                data_buf++;
            }
        }
        break;

    case BUF_BGRA:
        for (int i = 0; i < h; i++) {
            unsigned char *datap = &data[start + i*width*3];
            for (int j = 0; j < w; j++) {
                *datap++ = *(data_buf+2);
                *datap++ = *(data_buf+1);
                *datap++ = *data_buf;
                data_buf += 4;
            }
        }
        break;

    default:
        throw "Unexpected buf_type in FixedJpegStack::Push";
    }
}


void
FixedJpegStack::SetQuality(int q)
{
    quality = q;
}

NAN_METHOD(FixedJpegStack::New)
{
    ;

    if (info.Length() < 2) {
        Nan::ThrowError("At least two arguments required - width, height, [and buffer type]");
    }
    if (!info[0]->IsInt32()) {
        Nan::ThrowError("First argument must be integer width.");
    }
    if (!info[1]->IsInt32()) {
        Nan::ThrowError("Second argument must be integer height.");
    }

    int w = Nan::To<int32_t>(info[0]).FromJust();
    int h = Nan::To<int32_t>(info[1]).FromJust();

    if (w < 0) {
        Nan::ThrowError("Width can't be negative.");
    }
    if (h < 0) {
        Nan::ThrowError("Height can't be negative.");
    }

    buffer_type buf_type = BUF_RGB;
    if (info.Length() == 3) {
        if (!info[2]->IsString()) {
            Nan::ThrowError("Third argument must be a string. Either 'rgb', 'bgr', 'rgba' or 'bgra'.");
        }

        Nan::Utf8String bt(info[2]->ToString());
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

    try {
        FixedJpegStack *jpeg = new FixedJpegStack(w, h, buf_type);
        jpeg->Wrap(info.This());
        info.GetReturnValue().Set(info.This());
    }
    catch (const char *err) {
        Nan::ThrowError(err);
    }
}

NAN_METHOD(FixedJpegStack::JpegEncodeSync)
{
    ;
    FixedJpegStack *jpeg = Nan::ObjectWrap::Unwrap<FixedJpegStack>(info.This());
    try {
        info.GetReturnValue().Set(jpeg->JpegEncodeSync());
    } catch (const char *err) {
        Nan::ThrowError(err);
    }
}

NAN_METHOD(FixedJpegStack::Push)
{
    ;

    if (!node::Buffer::HasInstance(info[0])) {
        Nan::ThrowError("First argument must be Buffer.");
    }
    if (!info[1]->IsInt32()) {
        Nan::ThrowError("Second argument must be integer x.");
    }
    if (!info[2]->IsInt32()) {
        Nan::ThrowError("Third argument must be integer y.");
    }
    if (!info[3]->IsInt32()) {
        Nan::ThrowError("Fourth argument must be integer w.");
    }
    if (!info[4]->IsInt32()) {
        Nan::ThrowError("Fifth argument must be integer h.");
    }

    FixedJpegStack *jpeg = Nan::ObjectWrap::Unwrap<FixedJpegStack>(info.This());
    Local<Object> data_buf = info[0]->ToObject();
    int x = Nan::To<int32_t>(info[1]).FromJust();
    int y = Nan::To<int32_t>(info[2]).FromJust();
    int w = Nan::To<int32_t>(info[3]).FromJust();
    int h = Nan::To<int32_t>(info[4]).FromJust();

    if (x < 0) {
        Nan::ThrowError("Coordinate x smaller than 0.");
    }
    if (y < 0) {
        Nan::ThrowError("Coordinate y smaller than 0.");
    }
    if (w < 0) {
        Nan::ThrowError("Width smaller than 0.");
    }
    if (h < 0) {
        Nan::ThrowError("Height smaller than 0.");
    }
    if (x >= jpeg->width) {
        Nan::ThrowError("Coordinate x exceeds FixedJpegStack's dimensions.");
    }
    if (y >= jpeg->height) {
        Nan::ThrowError("Coordinate y exceeds FixedJpegStack's dimensions.");
    }
    if (x+w > jpeg->width) {
        Nan::ThrowError("Pushed fragment exceeds FixedJpegStack's width.");
    }
    if (y+h > jpeg->height) {
        Nan::ThrowError("Pushed fragment exceeds FixedJpegStack's height.");
    }

    jpeg->Push((unsigned char *)node::Buffer::Data(data_buf), x, y, w, h);

    info.GetReturnValue().SetUndefined();
}

NAN_METHOD(FixedJpegStack::SetQuality)
{
    ;

    if (info.Length() != 1) {
        Nan::ThrowError("One argument required - quality");
    }

    if (!info[0]->IsInt32()) {
        Nan::ThrowError("First argument must be integer quality");
    }

    int q = Nan::To<int32_t>(info[0]).FromJust();

    if (q < 0) {
        Nan::ThrowError("Quality must be greater or equal to 0.");
    }
    if (q > 100) {
        Nan::ThrowError("Quality must be less than or equal to 100.");
    }

    FixedJpegStack *jpeg = Nan::ObjectWrap::Unwrap<FixedJpegStack>(info.This());
    jpeg->SetQuality(q);

    info.GetReturnValue().SetUndefined();
}

void
FixedJpegStack::UV_JpegEncode(uv_work_t *req)
{
    encode_request *enc_req = (encode_request *)req->data;
    FixedJpegStack *jpeg = (FixedJpegStack *)enc_req->jpeg_obj;

    try {
        JpegEncoder encoder(jpeg->data, jpeg->width, jpeg->height, jpeg->quality, BUF_RGB);
        encoder.encode();
        enc_req->jpeg_len = encoder.get_jpeg_len();
        enc_req->jpeg = (char *)malloc(sizeof(*enc_req->jpeg)*enc_req->jpeg_len);
        if (!enc_req->jpeg) {
            enc_req->error = strdup("malloc in FixedJpegStack::UV_JpegEncode failed.");
            return;
        }
        else {
            memcpy(enc_req->jpeg, encoder.get_jpeg(), enc_req->jpeg_len);
        }
    }
    catch (const char *err) {
        enc_req->error = strdup(err);
    }
}

void 
FixedJpegStack::UV_JpegEncodeAfter(uv_work_t *req)
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

    enc_req->callback->Call(2, argv);

    delete enc_req->callback;
    free(enc_req->jpeg);
    free(enc_req->error);

    ((FixedJpegStack *)enc_req->jpeg_obj)->Unref();
    free(enc_req);
}

NAN_METHOD(FixedJpegStack::JpegEncodeAsync)
{
    ;

    if (info.Length() != 1) {
        Nan::ThrowError("One argument required - callback function.");
    }

    if (!info[0]->IsFunction()) {
        Nan::ThrowError("First argument must be a function.");
    }

    Local<Function> callback = info[0].As<Function>();
    FixedJpegStack *jpeg = Nan::ObjectWrap::Unwrap<FixedJpegStack>(info.This());

    encode_request *enc_req = (encode_request *)malloc(sizeof(*enc_req));
    if (!enc_req) {
        Nan::ThrowError("malloc in FixedJpegStack::JpegEncodeAsync failed.");
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

