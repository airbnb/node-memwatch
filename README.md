`node-memwatch`: Leak Detection and Heap Diffing for Node.JS
============================================================

`node-memwatch` is here to help you detect and find memory leaks in
Node.JS code.  It provides:

- A `stats` event, emitted on full MarkSweepCompact GCs giving you
  data describing your heap usage and trends over time.

- A `HeapDiff` class that lets you compare the state of your heap between
  two points in time, telling you what has been allocated, and what
  has been released.


Installation
------------

- `npm install @aegenet/node-memwatch`


Description
-----------

There are a growing number of tools for debugging and profiling memory
usage in Node.JS applications, but there is still a need for a
platform-independent native module that requires no special
instrumentation.  This module attempts to satisfy that need.

To get started, import `node-memwatch` like so:

```javascript
var memwatch = require('@aegenet/node-memwatch');
```

### Leak Detection

Currently unsupported while we explore heuristics

### Heap Usage

The best way to evaluate your memory footprint is to look at heap
usage right after V8 performs garbage collection.  `memwatch` does
exactly this - it checks heap usage only after GC to give you a stable
baseline of your actual memory usage.

When V8 performs a garbage collection (technically, we're talking
about a full GC with heap compaction), `memwatch` will emit a `stats`
event.

```javascript
memwatch.on('stats', function(stats) { ... });
```

The `stats` data will look something like this:

```javascript
{
  gcScavengeCount: 1,
  gcScavengeTime: 1100880, // ns
  gcMarkSweepCompactCount: 2,
  gcMarkSweepCompactTime: 21157231, // ns
  gcIncrementalMarkingCount: 0,
  gcIncrementalMarkingTime: 0, //ns
  gcProcessWeakCallbacksCount: 0,
  gcProcessWeakCallbacksTime: 0, // ns
  total_heap_size: 16097280, // bytes
  total_heap_size_executable: 3670016, // bytes
  total_physical_size: 10741880, // bytes
  total_available_size: 1487689928, // bytes
  used_heap_size: 5691584, // bytes
  heap_size_limit: 1501560832, // bytes
  malloced_memory: 8192,
  peak_malloced_memory: 1185464,
  gc_time: 4587251 // ns
}
```

V8 has its own idea of when it's best to perform a GC, and under a
heavy load, it may defer this action for some time.  To aid in
speedier debugging, `memwatch` provides a `gc()` method to force V8 to
do a full GC and heap compaction.


### Heap Diffing

For leak isolation, it provides a `HeapDiff` class that takes two snapshots and
computes a diff between them.  For example:

```javascript
// Take first snapshot
var hd = new memwatch.HeapDiff();

// do some things ...

// Take the second snapshot and compute the diff
var diff = hd.end();
```

The contents of `diff` will look something like:

```javascript
{
  "before": { "nodes": 11625, "size_bytes": 1869904, "size": "1.78 mb" },
  "after":  { "nodes": 21435, "size_bytes": 2119136, "size": "2.02 mb" },
  "change": { "size_bytes": 249232, "size": "243.39 kb", "freed_nodes": 197,
    "allocated_nodes": 10007,
    "details": [
      { "what": "String",
        "size_bytes": -2120,  "size": "-2.07 kb",  "+": 3,    "-": 62
      },
      { "what": "Array",
        "size_bytes": 66687,  "size": "65.13 kb",  "+": 4,    "-": 78
      },
      { "what": "LeakingClass",
        "size_bytes": 239952, "size": "234.33 kb", "+": 9998, "-": 0
      }
    ]
  }
}
```

The diff shows that during the sample period, the total number of
allocated `String` and `Array` classes decreased, but `Leaking Class`
grew by 9998 allocations.  Hmmm.

You can use `HeapDiff` in your `on('stats')` callback; even though it
takes a memory snapshot, which triggers a V8 GC, it will not trigger
the `stats` event itself.  Because that would be silly.


Future Work
-----------

Please see the Issues to share suggestions and contribute!


License
-------

http://wtfpl.net
