#include <nan.h>
#include <node.h>
#include <node_buffer.h>
#include <jpeglib.h>
#include <cstdlib>
#include <cstring>

#include "common.h"
#include "dynamic_jpeg_stack.h"
#include "jpeg_encoder.h"

using v8::Object;
using v8::Handle;
using v8::Local;
using v8::Function;
using v8::FunctionTemplate;
using v8::String;

void
DynamicJpegStack::Initialize(v8::Handle<v8::Object> target)
{
    Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    Nan::SetPrototypeMethod(t, "encode", JpegEncodeAsync);
    Nan::SetPrototypeMethod(t, "encodeSync", JpegEncodeSync);
    Nan::SetPrototypeMethod(t, "push", Push);
    Nan::SetPrototypeMethod(t, "reset", Reset);
    Nan::SetPrototypeMethod(t, "setBackground", SetBackground);
    Nan::SetPrototypeMethod(t, "setQuality", SetQuality);
    Nan::SetPrototypeMethod(t, "dimensions", Dimensions);
    Nan::Set(target, Nan::New("DynamicJpegStack").ToLocalChecked(), Nan::GetFunction(t).ToLocalChecked());
}

DynamicJpegStack::DynamicJpegStack(buffer_type bbuf_type) :
    quality(60), buf_type(bbuf_type),
    dyn_rect(-1, -1, 0, 0),
    bg_width(0), bg_height(0), data(NULL) {}

DynamicJpegStack::~DynamicJpegStack()
{
    free(data);
}

void
DynamicJpegStack::update_optimal_dimension(int x, int y, int w, int h)
{
    if (dyn_rect.x == -1 || x < dyn_rect.x)
        dyn_rect.x = x;
    if (dyn_rect.y == -1 || y < dyn_rect.y)
        dyn_rect.y = y;
    
    if (dyn_rect.w == 0)
        dyn_rect.w = w;
    if (dyn_rect.h == 0)
        dyn_rect.h = h;

    int ww = w - (dyn_rect.w - (x - dyn_rect.x));
    if (ww > 0)
        dyn_rect.w += ww;

    int hh = h - (dyn_rect.h - (y - dyn_rect.y));
    if (hh > 0)
        dyn_rect.h += hh;
}

Handle<Value>
DynamicJpegStack::JpegEncodeSync()
{
    JpegEncoder jpeg_encoder(data, bg_width, bg_height, quality, BUF_RGB);
    jpeg_encoder.setRect(Rect(dyn_rect.x, dyn_rect.y, dyn_rect.w, dyn_rect.h));
    jpeg_encoder.encode();
    Handle<Object> retbuf = Nan::CopyBuffer((const char*) jpeg_encoder.get_jpeg(), jpeg_encoder.get_jpeg_len()).ToLocalChecked();
    return retbuf;
}

void
DynamicJpegStack::Push(unsigned char *data_buf, int x, int y, int w, int h)
{
    update_optimal_dimension(x, y, w, h);

    int start = y*bg_width*3 + x*3;

    switch (buf_type) {
    case BUF_RGB:
        for (int i = 0; i < h; i++) {
            unsigned char *datap = &data[start + i*bg_width*3];
            for (int j = 0; j < w; j++) {
                *datap++ = *data_buf++;
                *datap++ = *data_buf++;
                *datap++ = *data_buf++;
            }
        }
        break;

    case BUF_BGR:
        for (int i = 0; i < h; i++) {
            unsigned char *datap = &data[start + i*bg_width*3];
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
            unsigned char *datap = &data[start + i*bg_width*3];
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
            unsigned char *datap = &data[start + i*bg_width*3];
            for (int j = 0; j < w; j++) {
                *datap++ = *(data_buf+2);
                *datap++ = *(data_buf+1);
                *datap++ = *data_buf;
                data_buf += 4;
            }
        }
        break;

    default:
        throw "Unexpected buf_type in DynamicJpegStack::Push";
    }
}

void
DynamicJpegStack::SetBackground(unsigned char *data_buf, int w, int h)
{
    if (data) {
        free(data);
        data = NULL;
    }

    switch (buf_type) {
    case BUF_RGB:
        data = (unsigned char *)malloc(sizeof(*data)*w*h*3);
        if (!data) throw "malloc failed in DynamicJpegStack::SetBackground";
        memcpy(data, data_buf, w*h*3);
        break;

    case BUF_BGR:
        data = bgr_to_rgb(data_buf, w*h*3);
        if (!data) throw "malloc failed in DynamicJpegStack::SetBackground";
        break;

    case BUF_RGBA:
        data = rgba_to_rgb(data_buf, w*h*4);
        if (!data) throw "malloc failed in DynamicJpegStack::SetBackground";
        break;

    case BUF_BGRA:
        data = bgra_to_rgb(data_buf, w*h*4);
        if (!data) throw "malloc failed in DynamicJpegStack::SetBackground";
        break;

    default:
        throw "Unexpected buf_type in DynamicJpegStack::SetBackground";
    }
    bg_width = w;
    bg_height = h;
}

void
DynamicJpegStack::SetQuality(int q)
{
    quality = q;
}

void
DynamicJpegStack::Reset()
{
    dyn_rect = Rect(-1, -1, 0, 0);
}

Handle<Value>
DynamicJpegStack::Dimensions()
{
    Handle<Object> dim = Nan::New<Object>();
    Nan::Set(dim, Nan::New("x").ToLocalChecked(), Nan::New(dyn_rect.x));
    Nan::Set(dim, Nan::New("y").ToLocalChecked(), Nan::New(dyn_rect.y));
    Nan::Set(dim, Nan::New("width").ToLocalChecked(), Nan::New(dyn_rect.w));
    Nan::Set(dim, Nan::New("height").ToLocalChecked(), Nan::New(dyn_rect.h));
    return dim;
}

NAN_METHOD(DynamicJpegStack::New)
{
    ;

    if (info.Length() > 1) {
        Nan::ThrowError("One argument max - buffer type.");
    }

    buffer_type buf_type = BUF_RGB;
    if (info.Length() == 1) {
        if (!info[0]->IsString()) {
            Nan::ThrowError("First argument must be a string. Either 'rgb', 'bgr', 'rgba' or 'bgra'.");
        }

        Nan::Utf8String bt(info[0]->ToString());
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

    DynamicJpegStack *jpeg = new DynamicJpegStack(buf_type);
    jpeg->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
}

NAN_METHOD(DynamicJpegStack::JpegEncodeSync)
{
    ;
    DynamicJpegStack *jpeg = Nan::ObjectWrap::Unwrap<DynamicJpegStack>(info.This());
    try {
        info.GetReturnValue().Set(jpeg->JpegEncodeSync());
    }
    catch (const char *err) {
        Nan::ThrowError(err);
    }
}

NAN_METHOD(DynamicJpegStack::Push)
{
    ;

    if (info.Length() != 5) {
        Nan::ThrowError("Five arguments required - buffer, x, y, width, height.");
    }

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

    DynamicJpegStack *jpeg = Nan::ObjectWrap::Unwrap<DynamicJpegStack>(info.This());

    if (!jpeg->data)
        Nan::ThrowError("No background has been set, use setBackground or setSolidBackground to set.");

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
    if (x >= jpeg->bg_width) {
        Nan::ThrowError("Coordinate x exceeds DynamicJpegStack's background dimensions.");
    }
    if (y >= jpeg->bg_height) {
        Nan::ThrowError("Coordinate y exceeds DynamicJpegStack's background dimensions.");
    }
    if (x+w > jpeg->bg_width) {
        Nan::ThrowError("Pushed fragment exceeds DynamicJpegStack's width.");
    }
    if (y+h > jpeg->bg_height) {
        Nan::ThrowError("Pushed fragment exceeds DynamicJpegStack's height.");
    }

    jpeg->Push((unsigned char *)node::Buffer::Data(data_buf), x, y, w, h);

    info.GetReturnValue().SetUndefined();
}

NAN_METHOD(DynamicJpegStack::SetBackground)
{
    ;

    if (info.Length() != 3)
        Nan::ThrowError("Four arguments required - buffer, width, height");
    if (!node::Buffer::HasInstance(info[0]))
        Nan::ThrowError("First argument must be Buffer.");
    if (!info[1]->IsInt32())
        Nan::ThrowError("Second argument must be integer width.");
    if (!info[2]->IsInt32())
        Nan::ThrowError("Third argument must be integer height.");

    DynamicJpegStack *jpeg = Nan::ObjectWrap::Unwrap<DynamicJpegStack>(info.This());
    Local<Object> data_buf = info[0]->ToObject();
    int w = Nan::To<int32_t>(info[1]).FromJust();
    int h = Nan::To<int32_t>(info[2]).FromJust();

    if (w < 0)
        Nan::ThrowError("Coordinate x smaller than 0.");
    if (h < 0)
        Nan::ThrowError("Coordinate y smaller than 0.");

    try {
        jpeg->SetBackground((unsigned char *)node::Buffer::Data(data_buf), w, h);
    }
    catch (const char *err) {
        Nan::ThrowError(err);
    }

    info.GetReturnValue().SetUndefined();
}

NAN_METHOD(DynamicJpegStack::Reset)
{
    ;

    DynamicJpegStack *jpeg = Nan::ObjectWrap::Unwrap<DynamicJpegStack>(info.This());
    jpeg->Reset();
    info.GetReturnValue().SetUndefined();
}

NAN_METHOD(DynamicJpegStack::Dimensions)
{
    ;

    DynamicJpegStack *jpeg = Nan::ObjectWrap::Unwrap<DynamicJpegStack>(info.This());
    info.GetReturnValue().Set(jpeg->Dimensions());
}

NAN_METHOD(DynamicJpegStack::SetQuality)
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

    DynamicJpegStack *jpeg = Nan::ObjectWrap::Unwrap<DynamicJpegStack>(info.This());
    jpeg->SetQuality(q);

    info.GetReturnValue().SetUndefined();
}

void
DynamicJpegStack::UV_JpegEncode(uv_work_t *req)
{
    encode_request *enc_req = (encode_request *)req->data;
    DynamicJpegStack *jpeg = (DynamicJpegStack *)enc_req->jpeg_obj;

    try {
        Rect &dyn_rect = jpeg->dyn_rect;
        JpegEncoder encoder(jpeg->data, jpeg->bg_width, jpeg->bg_height, jpeg->quality, BUF_RGB);
        encoder.setRect(Rect(dyn_rect.x, dyn_rect.y, dyn_rect.w, dyn_rect.h));
        encoder.encode();
        enc_req->jpeg_len = encoder.get_jpeg_len();
        enc_req->jpeg = (char *)malloc(sizeof(*enc_req->jpeg)*enc_req->jpeg_len);
        if (!enc_req->jpeg) {
            enc_req->error = strdup("malloc in DynamicJpegStack::UV_JpegEncode failed.");
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
DynamicJpegStack::UV_JpegEncodeAfter(uv_work_t *req)
{
    ;

    encode_request *enc_req = (encode_request *)req->data;
    delete req;
    DynamicJpegStack *jpeg = (DynamicJpegStack *)enc_req->jpeg_obj;

    Handle<Value> argv[3];

    if (enc_req->error) {
        argv[0] = Nan::Undefined();
        argv[1] = Nan::Undefined();
        argv[2] = Nan::Error(enc_req->error);
    }
    else {
        Handle<Object> buf = Nan::NewBuffer(enc_req->jpeg, enc_req->jpeg_len).ToLocalChecked();
        argv[0] = buf;
        argv[1] = jpeg->Dimensions();
        argv[2] = Nan::Undefined();
    }

    enc_req->callback->Call(3, argv);

    delete enc_req->callback;
    free(enc_req->jpeg);
    free(enc_req->error);

    jpeg->Unref();
    free(enc_req);
}

NAN_METHOD(DynamicJpegStack::JpegEncodeAsync)
{
    ;

    if (info.Length() != 1) {
        Nan::ThrowError("One argument required - callback function.");
    }

    if (!info[0]->IsFunction()) {
        Nan::ThrowError("First argument must be a function.");
    }

    Local<Function> callback = Local<Function>::Cast(info[0]);
    DynamicJpegStack *jpeg = Nan::ObjectWrap::Unwrap<DynamicJpegStack>(info.This());

    encode_request *enc_req = (encode_request *)malloc(sizeof(*enc_req));
    if (!enc_req) {
        Nan::ThrowError("malloc in DynamicJpegStack::JpegEncodeAsync failed.");
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

