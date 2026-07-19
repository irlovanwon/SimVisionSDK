# API Module

## General Rules

### ZMQ Transport

Both `tcp` and `ipc` shall be supported (configured via endpoint strings in `config.json`).

### HTTPS Server (API 1a)

Use **Hybrid (Pools) of Event Loop and Thread-per-Request** — 1 accept thread + N worker threads (default 4). TLS mandatory (OpenSSL). Bounded pending queue; reject with 503 if exceeded.

### Shared Pointers for Dataflow

Internal data transfer uses `std::shared_ptr` (no large copies). See [AGENTS.md](AGENTS.md).

---

## API 1: SimVisionSDK ↔ Client (StereoCamera)

SimVisionSDK is the **camera simulator**. Clients communicate with it via API 1.

### API 1a — Command Channel

| Property | Value |
|----------|-------|
| Role | SimVisionSDK = HTTPS Server |
| Protocol | HTTPS (OpenSSL TLS, HTTP/1.1, JSON body) |
| Direction | Client → SimVisionSDK (requests); SimVisionSDK → Client (responses) |
| Port | 8443 (configurable) |

#### HTTP Endpoints

REST-style paths are accepted, and a JSON body with a `"command"` field is also accepted (backward-compatible). Paths are case-insensitive and normalized to `snake_case`.

| Method | Path | Body | Description |
|--------|------|------|-------------|
| GET/POST | `/CheckStatus` | — | System status: initialized, sessions, channels, pipeline, publisher |
| POST | `/Connect` | `{}` | Start a client session (returns `client_id`) |
| POST | `/Disconnect` | `{"client_id":...}` | End session, unsubscribe all |
| POST | `/StartCapture` | `{"client_id":...,"data_types":[...]}` | Begin capturing data types |
| POST | `/StopCapture` | `{"client_id":...,"data_types":[...]}` | Stop capturing data types |
| POST | `/ListChannels` | `{}` | All channels with supported/activated flags |
| POST | `/ListParameters` | `{}` | All supported parameters with ranges |
| POST | `/GetParameter` | `{"name":...}` | Get one parameter value |
| POST | `/SetParameter` | `{"name":...,"value":...}` | Set parameter |
| POST | `/ActivateChannel` | `{"data_type":...}` | Force-activate a single data type |
| POST | `/DeactivateChannel` | `{"data_type":...}` | Deactivate a single data type |
| POST | `/Init` | — | No-op → returns `code=3` (Already initialized) |
| POST | `/Dispose` | — | Rejected → returns `code=1` (Not permitted via API) |

#### Response Envelope

```json
{
  "code": 0,
  "message": "Success",
  "data": {
    ... payload ...,
    "_frame": {
      "client_id": 1,
      "client_addr": "127.0.0.1",
      "server_id": "sim_vision_01",
      "command": "start_capture",
      "recv_ts": "20260719-204843-914",
      "recv_unix_ms": 1784465323914,
      "send_ts": "20260719-204843-914",
      "send_unix_ms": 1784465323914
    }
  }
}
```

`recv_ts`/`send_ts` use the `YYYYMMDD-HHMMSS-sss` log format; the `_unix_ms` fields are Unix epoch milliseconds.

> `init`/`dispose` are internal lifecycle operations. The service auto-initializes at startup and auto-disposes at shutdown. Clients use `Connect`/`Disconnect` for session management.

### API 1b — Data Channel

| Property | Value |
|----------|-------|
| Role | SimVisionSDK = ZMQ Publisher (PUB) |
| Protocol | ZMQ PUB (tcp or ipc) |
| Direction | SimVisionSDK → Client |
| Clients | Multiple simultaneous ZMQ SUB clients (PUB fan-out) |
| Encoding | **Raw bytes only — no encoding** |
| Socket options | `ZMQ_IMMEDIATE=1`, `ZMQ_DONTWAIT`, `ZMQ_SNDHWM` (config) |

#### Data Categories

| Group | Topic | Data Types |
|-------|-------|-----------|
| Visual & Geometric 2D | `visual_geometric_2d` | StereoImage, DepthMap |
| Visual & Geometric 3D | `visual_geometric_3d` | PointCloud, DisparityMap, ConfidenceMap |
| Sensor & Tracking | `sensor_tracking` | IMU, Temperature, Magnetometer, Barometer |

#### ZMQ Message Format (multipart, raw)

```
[topic (string)] [JSON header] [payload 1] ... [payload n]
```

JSON header:

```json
{
  "group": "visual_2d",
  "channel": "visual_geometric_2d",
  "pub_id": "sim_vision_pub_01",
  "ts_sec": 1784465323,
  "ts_nsec": 914000000,
  "frame_index": 42,
  "source": "SimVisionSDK/1.0",
  "parts": [
    {"id":"stereo_image","size":N,"ts_sec":...,"ts_nsec":...},
    {"id":"depth_map","size":M,"ts_sec":...,"ts_nsec":...}
  ]
}
```

- `ts_sec`/`ts_nsec` captured at the moment of image read (capture tick).
- Payloads are raw bytes; payload frames are sent zero-copy (`zmq_msg_init_data`).
- Group names in `start_capture`/`activate_channel` (`visual_2d`, `visual_3d`, `sensor_data`) expand to member data types.

---

## Channel Selection Strategy

Subscriber-driven: data only flows on channels that have at least one subscriber.

| Command | Effect |
|---------|--------|
| `StartCapture(client_id, data_types)` | Per-type subscriber refcount++. CaptureEngine starts producing those types. |
| `StopCapture(client_id, data_types)` | Per-type refcount--. When 0 and not force-active → type stops; stale data flushed. |
| `ActivateChannel(data_type)` | Force-active flag set (independent of subscribers). |
| `DeactivateChannel(data_type)` | Force-active cleared. |

Rules:

1. **No subscribers → no transfer.** Inactive types are not produced.
2. **One capture, many clients.** Multiple subscribers share one PUB stream (ZMQ fan-out).
3. **Disconnect cleanup.** `Disconnect` removes all of that client's subscriptions.
4. **Last subscriber leaves → stale flush.** SPSC queues + ZMQ reliance (IMMEDIATE) clear stale data.
