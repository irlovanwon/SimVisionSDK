# Thread & Data Flow

## 1. Thread Map

| Thread | API | Count | Strategy | Responsibility |
|--------|-----|-------|----------|----------------|
| Main | — | 1 | — | Signal handling, wiring, graceful shutdown |
| AdminServer accept | API 1a | 1 | Event loop (accept) | Accept TLS connections, enqueue to workers |
| AdminServer worker | API 1a | N (default 4) | Thread-per-request | TLS handshake, HTTP parse, command dispatch, response |
| CaptureEngine | core | 1 | CV-paced loop (`wait_for`) | Round-robin sample data, synthesize, push to SPSC |
| DataPublisher | API 1b | 1 | CV-driven SPSC consumer (`wait_for` timeout) | Pop ChannelFrames, ZMQ PUB multipart (zero-copy) |

### Data Transfer Mechanisms

| Segment | Mechanism | Drop Policy |
|---------|-----------|-------------|
| CaptureEngine → SPSC | 3 SPSC queues (`shared_ptr<ChannelFrame>`) | Drop-NEWEST |
| SPSC → DataPublisher | `condition_variable::wait_for` (timeout) | low-power wait |
| DataPublisher → ZMQ PUB | `zmq_msg_init_data` zero-copy + `ZMQ_DONTWAIT` | Drop-NEWEST at HWM |

## 2. Data Flow

```
 SimDataSource (RAM)                         [RAW — no encoding]
   ├── stereo pairs (L_001..R_001..) round-robin
   └── synthetic: depth / disparity / confidence / pointcloud / imu / temp / mag / baro
        │
        ▼
 ┌──────────────────────────────────────────────────────────┐
 │  CaptureEngine (1 thread, FPS-paced via CV wait_for)     │
 │  ├── for each active group (subscriber-driven):          │
 │  │     build ChannelFrame (ts captured at image read)    │
 │  │     push shared_ptr<ChannelFrame> → SPSC              │
 │  └── pace: wait_for(period - elapsed)                    │
 └──────────────────────────────────────────────────────────┘
        │ SPSC (3 queues: visual_2d / visual_3d / sensor_data)
        ▼
 ┌──────────────────────────────────────────────────────────┐
 │  DataPublisher (1 thread, CV-driven consumer)            │
 │  ├── pop_all(timeout) from all 3 queues                  │
 │  ├── for each frame: send multipart                      │
 │  │     [topic][JSON header][payload1..n] (zero-copy)     │
 │  └── ZMQ_DONTWAIT — drop-NEWEST at HWM                   │
 └──────────────────────────────────────────────────────────┘
        │ ZMQ PUB x3 (grouped, zero-copy)
        ▼
   Client (StereoCamera) ZMQ SUB
       visual_geometric_2d  → [hdr][stereo L+R][depth f32]
       visual_geometric_3d  → [hdr][pc f32×4][disp f32][conf f32]
       sensor_tracking      → [hdr][imu JSON][temp JSON][mag JSON][baro JSON]
```

## 3. Command Flow

```
Client (StereoCamera)
   │ HTTPS POST /StartCapture {client_id, data_types}
   ▼
AdminServer (accept → worker) → CommandHandler
   ├── ChannelManager.start_capture(client_id, types)
   ├── (subscriber refcount++)
   └── CaptureEngine now produces those types on next tick
```

## 4. Dynamic Lifecycle (Subscriber-Driven)

`ChannelManager` tracks per-client subscriptions:

```
struct State { int subscriber_count; bool force_active; };
is_active(type) = subscriber_count > 0 || force_active
```

- `start_capture` → per-type refcount++ (only first time a client subscribes to a type).
- `stop_capture` → per-type refcount-- (only if that client had it).
- `activate_channel` → force_active = true (independent of subscribers).
- `deactivate_channel` → force_active = false.
- `disconnect` → removes all of the client's subscriptions.

CaptureEngine only emits groups where at least one type is active. A type with no subscribers and not force-active is **not** produced — saving CPU and bandwidth.

## 5a. Service Restart Handling

On SIGTERM/SIGINT, before teardown:

```
Shutdown signal
  │
  ├── 1. DataPublisher::publish_shutdown()
  │      └── ZMQ multipart `{"event":"server_shutdown"}` on all 3 PUB channels
  ├── 2. 500 ms drain (clients process notification + reset)
  ├── 3. AdminServer::stop() — close accept + workers
  ├── 4. CaptureEngine::stop() — join capture thread
  ├── 5. DataPipeline::drain() — clear SPSC
  ├── 6. DataPublisher::stop() — close PUB sockets (ZMQ_LINGER)
  └── 7. zmq_ctx_destroy()
```

Clients receiving `server_shutdown` reset their session/connection state and reconnect when the service is available again.

## 5b. Stale Data Handling

When data flow stops (`StopCapture` / `Disconnect` / `DeactivateChannel`), stale data is flushed so new clients never receive old frames:

```
StopCapture / Disconnect / DeactivateChannel
  │
  ├── if no subscribers remain (anywhere):
  │     ├── DataPipeline::drain()  — pop all 3 SPSC queues
  │     └── DataPublisher::drain() — SPSC cleared; ZMQ relies on IMMEDIATE
  │           (no subscriber → no outbound buffering)
  │
  └── if a whole group goes inactive:
        └── DataPipeline::drain_group(group)
```

ZMQ PUB with `ZMQ_IMMEDIATE=1` does not queue messages when no subscriber has completed the handshake, so when the last subscriber leaves the ZMQ outbound buffer naturally stops accumulating — combined with the explicit SPSC drain this clears the pipeline.
