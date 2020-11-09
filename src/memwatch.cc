/*
 * 2012|lloyd|http://wtfpl.org
 */

#include "platformcompat.hh"
#include "memwatch.hh"
#include "heapdiff.hh"
#include "util.hh"

#include <node.h>
#include <node_version.h>

#include <string>
#include <cstring>
#include <sstream>

#include <math.h> // for pow
#include <time.h> // for time

using namespace v8;
using namespace node;

Local<Object> g_context;

class UponGCCallback : public Nan::AsyncResource {
    public:
    UponGCCallback(v8::Local<v8::Function> callback_) : Nan::AsyncResource("memwatch:upon_gc") {
        callback.Reset(callback_);
    }
    ~UponGCCallback() {
        callback.Reset();
    }

    void Call(int argc, Local<v8::Value> argv[]) {
        v8::Isolate *isolate = v8::Isolate::GetCurrent();
        runInAsyncScope(isolate->GetCurrentContext()->Global(), Nan::New(callback), argc, argv);
    }

    private:
    Nan::Persistent<v8::Function> callback;
};

UponGCCallback *uponGCCallback = NULL;

struct GCStats {
    // counts of different types of gc events
    size_t gcScavengeCount;
    uint64_t gcScavengeTime;

    size_t gcMarkSweepCompactCount;
    uint64_t gcMarkSweepCompactTime;

    size_t gcIncrementalMarkingCount;
    uint64_t gcIncrementalMarkingTime;

    size_t gcProcessWeakCallbacksCount;
    uint64_t gcProcessWeakCallbacksTime;
};

struct Baton {
    uv_work_t req;
    size_t total_heap_size;
    size_t total_heap_size_executable;
    size_t total_physical_size;
    size_t total_available_size;
    size_t used_heap_size;
    size_t heap_size_limit;
    size_t malloced_memory;
    size_t peak_malloced_memory;

    GCStats stats;

    uint64_t gc_time;

    uint64_t gc_ts;

    GCType type;
    GCCallbackFlags flags;
};

static uint64_t currentGCStartTime = 0;
static GCStats s_stats = {0, 0, 0, 0, 0, 0, 0, 0};

static v8::Local<v8::Number> javascriptNumber(uint64_t n) {
    if (n < pow(2, 32)) {
        return Nan::New(static_cast<uint32_t>(n));
    } else {
        return Nan::New(static_cast<double>(n));
    }
}

static v8::Local<v8::Number> javascriptNumberSize(size_t n) {
    return javascriptNumber(static_cast<uint64_t>(n));
}

static void AsyncMemwatchAfter(uv_work_t* request) {
    Nan::HandleScope scope;

    Baton * b = (Baton *) request->data;
    // if there are any listeners, it's time to emit!
    if (uponGCCallback) {
        Local<Value> argv[2];

        Local<Object> stats = Nan::New<v8::Object>();

        Nan::Set(stats, Nan::New("gc_ts").ToLocalChecked(), javascriptNumber(b->gc_ts));

        Nan::Set(stats, Nan::New("gcScavengeCount").ToLocalChecked(), javascriptNumberSize(b->stats.gcScavengeCount));
        Nan::Set(stats, Nan::New("gcScavengeTime").ToLocalChecked(), javascriptNumber(b->stats.gcScavengeTime));
        Nan::Set(stats, Nan::New("gcMarkSweepCompactCount").ToLocalChecked(), javascriptNumberSize(b->stats.gcMarkSweepCompactCount));
        Nan::Set(stats, Nan::New("gcMarkSweepCompactTime").ToLocalChecked(), javascriptNumber(b->stats.gcMarkSweepCompactTime));
        Nan::Set(stats, Nan::New("gcIncrementalMarkingCount").ToLocalChecked(), javascriptNumberSize(b->stats.gcIncrementalMarkingCount));
        Nan::Set(stats, Nan::New("gcIncrementalMarkingTime").ToLocalChecked(), javascriptNumber(b->stats.gcIncrementalMarkingTime));
        Nan::Set(stats, Nan::New("gcProcessWeakCallbacksCount").ToLocalChecked(), javascriptNumberSize(b->stats.gcProcessWeakCallbacksCount));
        Nan::Set(stats, Nan::New("gcProcessWeakCallbacksTime").ToLocalChecked(), javascriptNumber(b->stats.gcProcessWeakCallbacksTime));

        Nan::Set(stats, Nan::New("total_heap_size").ToLocalChecked(), javascriptNumberSize(b->total_heap_size));
        Nan::Set(stats, Nan::New("total_heap_size_executable").ToLocalChecked(), javascriptNumberSize(b->total_heap_size_executable));
        Nan::Set(stats, Nan::New("total_physical_size").ToLocalChecked(), javascriptNumberSize(b->total_physical_size));
        Nan::Set(stats, Nan::New("total_available_size").ToLocalChecked(), javascriptNumberSize(b->total_available_size));
        Nan::Set(stats, Nan::New("used_heap_size").ToLocalChecked(), javascriptNumberSize(b->used_heap_size));
        Nan::Set(stats, Nan::New("heap_size_limit").ToLocalChecked(), javascriptNumberSize(b->heap_size_limit));
        Nan::Set(stats, Nan::New("malloced_memory").ToLocalChecked(), javascriptNumberSize(b->malloced_memory));
        Nan::Set(stats, Nan::New("peak_malloced_memory").ToLocalChecked(), javascriptNumberSize(b->peak_malloced_memory));
        Nan::Set(stats, Nan::New("gc_time").ToLocalChecked(), javascriptNumber(b->gc_time));

        // the type of event to emit
        argv[0] = Nan::New("stats").ToLocalChecked();
        argv[1] = stats;
        uponGCCallback->Call(2, argv);
    }

    delete b;
}

NAN_GC_CALLBACK(memwatch::before_gc) {
    currentGCStartTime = uv_hrtime();
}

static void noop_work_func(uv_work_t *) { }

NAN_GC_CALLBACK(memwatch::after_gc) {
    if (heapdiff::HeapDiff::InProgress()) return;

    uint64_t gcEnd = uv_hrtime();
    uint64_t gcTime = gcEnd - currentGCStartTime;

    switch(type) {
        case kGCTypeScavenge:
            s_stats.gcScavengeCount++;
            s_stats.gcScavengeTime += gcTime;
            return;
        case kGCTypeIncrementalMarking:
            s_stats.gcIncrementalMarkingCount++;
            s_stats.gcIncrementalMarkingTime += gcTime;
            return;
        case kGCTypeProcessWeakCallbacks:
            s_stats.gcProcessWeakCallbacksCount++;
            s_stats.gcProcessWeakCallbacksTime += gcTime;
            return;

        case kGCTypeMarkSweepCompact:
        case kGCTypeAll:
            break;
    }

    if (type == kGCTypeMarkSweepCompact) {
        s_stats.gcMarkSweepCompactCount++;
        s_stats.gcMarkSweepCompactTime += gcTime;

        Nan::HandleScope scope;

        Baton * baton = new Baton;
        v8::HeapStatistics hs;

        Nan::GetHeapStatistics(&hs);

        baton->gc_ts = uv_hrtime() / 1000;

        baton->total_heap_size = hs.total_heap_size();
        baton->total_heap_size_executable = hs.total_heap_size_executable();
        baton->total_physical_size = hs.total_physical_size();
        baton->total_available_size = hs.total_available_size();
        baton->used_heap_size = hs.used_heap_size();
        baton->heap_size_limit = hs.heap_size_limit();
        baton->malloced_memory = hs.malloced_memory();
        baton->peak_malloced_memory = hs.peak_malloced_memory();
        baton->gc_time = gcTime;
        baton->type = type;
        baton->flags = flags;
        baton->stats = s_stats;
        baton->req.data = (void *) baton;

        // schedule our work to run in a moment, once gc has fully completed.
        //
        // here we pass a noop work function to work around a flaw in libuv,
        // uv_queue_work on unix works fine, but will will crash on
        // windows.  see: https://github.com/joyent/libuv/pull/629
        uv_queue_work(uv_default_loop(), &(baton->req),
            noop_work_func, (uv_after_work_cb)AsyncMemwatchAfter);
    }
}

NAN_METHOD(memwatch::upon_gc) {
    Nan::HandleScope scope;
    if (info.Length() >= 1 && info[0]->IsFunction()) {
        uponGCCallback = new UponGCCallback(info[0].As<v8::Function>());
    }
    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(memwatch::trigger_gc) {
    Nan::HandleScope scope;
    int deadline_in_ms = 500;
    if (info.Length() >= 1 && info[0]->IsNumber()) {
    		deadline_in_ms = (int)(info[0]->Int32Value(Nan::GetCurrentContext()).FromJust());
    }
#if (NODE_MODULE_VERSION >= 0x002D)
    Nan::IdleNotification(deadline_in_ms);
    Nan::LowMemoryNotification();
#else
    while(!Nan::IdleNotification(deadline_in_ms)) {};
    Nan::LowMemoryNotification();
#endif
    info.GetReturnValue().Set(Nan::Undefined());
}
