# SimVisionSDK — Design Document

## 1. Overview

SimVisionSDK is a standalone C++17 service that **simulates a stereo camera SDK**. It loads sample data into RAM (stereo images) and synthesizes the remaining data (depth map, point cloud, disparity/confidence maps, IMU, temperature, magnetometer, barometer). It exposes an HTTPS admin API (API 1a) for session lifecycle + parameter management, and publishes raw frame data via ZMQ PUB (API 1b) on 3 grouped channels. Internal dataflow uses `std::shared_ptr` to avoid memory copy.

See also: [API Module](API_module.md) | [Commands](commands.md) | [Data](data.md) | [Thread & Data Flow](thread_dataflow.md) | [Status](status.md) | [Config](config.md) | [Folder Structure](folder_structure.md) | [Server Guide](server.md)

## 2. Tech Stack

| Layer | Technology |
|-------|-----------|
| Core Language | C++17 |
| Build System | CMake 3.16+ |
| Messaging | ZeroMQ (ZMQ) — IPC + TCP |
| Admin Protocol | HTTPS (TLS mandatory, OpenSSL) |
| Serialization | JSON (config + headers), raw ZMQ frames (data) |
| Config Format | JSON, `config/` directory |
| Testing | Google Test (gtest) |
| Target Platform | Linux aarch64 (EDGE01/02/03), Linux x86_64 (dev) |

## 3. Module Architecture

```
   ┌──────────────────────────────────────────────────────────┐
   │                    SimVisionSDK Service                  │
   │                                                          │
   │  ┌──────────────┐    round-robin     ┌────────────────┐  │
   │  │ SimDataSource│ ─────────────────► │ CaptureEngine  │  │
   │  │ (RAM images  │   synthetic gens   │ (FPS-paced,    │  │
   │  │  + generators)│                   │  sub-driven)   │  │
   │  └──────────────┘                    └───────┬────────┘  │
   │                                               │ shared_ptr │
   │                                               ▼            │
   │                                  ┌──────────────────────┐  │
   │                                  │ DataPipeline         │  │
   │                                  │  3 SPSC + CV         │  │
   │                                  └──────────┬───────────┘  │
   │                                             │ CV-driven     │
   │   API 1a (HTTPS Server, Hybrid Pools)       ▼               │
   │  ┌──────────────┐                ┌──────────────────────┐  │
   │  │ AdminServer  │◄── commands ───│ ChannelManager +     │  │
   │  │ CommandHandler│  /start_stop  │ SessionManager       │  │
   │  └──────────────┘                │ ParameterManager     │  │
   │                                   └──────────┬───────────┘  │
   └──────────────────────────────────────────────┼─────────────┘
                                                   │ ZMQ PUB
                                                   ▼
                                  Client (StereoCamera) subscribes
```

## 4. Source Code Structure

See [folder_structure.md](folder_structure.md).

### Module Map

| Module | Source Dir | Responsibility |
|--------|-----------|----------------|
| **Common** | `common/` | `DataType`/groups/timestamps, `DataBundle`, `ChannelFrame`, `ParameterManager`, `Response`, `Config`, `Logger` |
| **Data** | `data/` | `SPSCQueue` (lock-free), `DataPipeline` (3 queues + CV), `DataPublisher` (ZMQ PUB zero-copy), `ChannelManager` (subscriber tracking) |
| **DataSource** | `datasource/` | `SimDataSource` — stereo image loader + synthetic generators |
| **Capture** | `capture/` | `CaptureEngine` — FPS-paced subscriber-driven producer |
| **API** | `api/` | `AdminServer` (HTTPS hybrid pools), `CommandHandler`, `SessionManager` |

## 5. API Contracts

See [API_module.md](API_module.md) for full specification.

### API 1a — Command (HTTPS Server, Hybrid Pools)

SimVisionSDK = HTTPS Server. Clients (StereoCamera) send HTTPS POST/GET requests. 1 accept thread + N worker threads (default 4). Commands: connect, disconnect, start/stop_capture, check_status, list_channels, list_parameters, get/set_parameter, activate/deactivate_channel. `init` returns `code=3` (Already initialized); `dispose` returns `code=1` (Not permitted via API). Each response carries a `_frame` object: `{client_id, client_addr, server_id, command, recv_ts, send_ts}`.

### API 1b — Data (ZMQ PUB, 3 grouped channels)

SimVisionSDK = ZMQ Publisher. 3 grouped channels, ZMQ multipart `[topic][JSON header][payload1..n]`. Raw bytes only. Zero-copy via `zmq_msg_init_data` with a `shared_ptr<DataBundle>` keep-alive hint. `ZMQ_IMMEDIATE=1` (no slow-joiner), `ZMQ_DONTWAIT` (silent drop-NEWEST at HWM).

| Group | Topic | Data Types |
|-------|-------|-----------|
| Visual2D | `visual_geometric_2d` | StereoImage (L+R concatenated), DepthMap |
| Visual3D | `visual_geometric_3d` | PointCloud, DisparityMap, ConfidenceMap |
| SensorData | `sensor_tracking` | IMU, Temperature, Magnetometer, Barometer |

## 6. Data Types

See [data.md](data.md).

## 7. Parameters

| Parameter | Type | Default | R/W | needs_restart |
|-----------|------|---------|-----|---------------|
| `fps` | Integer (1..240) | 15 | R/W | yes |
| `exposure_time` | Integer (0..33333 μs) | 0 | R/W | no |
| `auto_exposure` | Enum (On/Off) | On | R/W | no |
| `gain_digital` | Integer (1..256) | 1 | R/W | no |
| `gain_analog` | Integer (1000..16000) | 1000 | R/W | no |
| `auto_gain` | Enum (On/Off) | On | R/W | no |
| `depth_mode` | Enum (NONE/NEURAL_LIGHT/NEURAL/NEURAL_PLUS/PERFORMANCE/QUALITY/ULTRA) | NEURAL | R/W | yes |

Every parameter carries `{value, min, max, default, is_readonly, is_available, needs_restart}`.

## 8. Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| Raw bytes only (no encoding) | API 1b is the simulator's output — raw file bytes + raw float32 + JSON sensors. Minimal CPU. |
| `std::shared_ptr` + lock-free SPSC | Zero-copy dataflow; drop-NEWEST SPSC never blocks capture. |
| Zero-copy ZMQ `zmq_msg_init_data` | Each payload frame wraps the bundle via a shared_ptr hint freed on send completion. |
| `ZMQ_IMMEDIATE=1` + `ZMQ_DONTWAIT` | No slow-joiner loss; silent drop-NEWEST at HWM; no buffering without a subscriber. |
| 3 grouped PUB channels | One multipart per group; matches the camera contract StereoCamera expects. |
| Subscriber-driven capture | `ChannelManager` refcount per client + `force_active`; capture only emits active groups. |
| `init`/`dispose` rejected via API | Lifecycle is managed by the service process (`main.cpp`); clients use connect/disconnect. |
| HTTPS hybrid pools | 1 accept + N workers; bounded pending queue (reject 503 if full). |
| Per-frame metadata | `_frame` on every response (API 1a); `pub_id` + image-read timestamp in ZMQ header (API 1b). |
| Service-restart broadcast | On SIGTERM/SIGINT, `publish_shutdown()` sends `{"event":"server_shutdown"}` on all 3 PUB channels, 500 ms drain, then teardown. |
| Stale-data flush | When the last subscriber leaves (or a whole group goes inactive), SPSC queues + ZMQ reliance (IMMEDIATE) clear stale data. |

## 9. Response Codes

| Code | Name | Meaning |
|------|------|---------|
| 0 | Success | Operation completed |
| 1 | Error | General failure / not permitted |
| 2 | NotReady | Precondition not met (e.g. not connected) |
| 3 | AlreadyInit | Already initialized (idempotent success) |
| 4 | InvalidParam | Parameter name/value invalid |
| 5 | Unavailable | Feature not supported |

## 10. Thread Model & Data Flow

See [thread_dataflow.md](thread_dataflow.md).

## 11. Key Constraints

| ID | Constraint |
|----|-----------|
| C1 | API 1b Data = ZMQ PUB, 3 grouped channels (raw bytes). |
| C2 | API 1a Admin = HTTPS (TLS mandatory), Hybrid Pools. |
| C3 | All ZMQ PUB sockets: `ZMQ_IMMEDIATE=1`, `ZMQ_DONTWAIT`, `ZMQ_SNDHWM` from config. |
| C4 | Internal dataflow uses `std::shared_ptr` (no large copies). |
| C5 | SPSC queues drop-NEWEST when full; consumers use `condition_variable::wait_for` with timeout. |
| C6 | Subscriber-driven: no subscribers for a type → that type is not produced. |
| C7 | Multiple clients sharing a type receive one capture stream (PUB fan-out). |
| C8 | `init`/`dispose` are internal lifecycle ops (rejected via API). |
| C9 | Stereo image published as L+R concatenated raw bytes. |
| C10 | Each API 1a response carries `_frame` metadata; each API 1b header carries `pub_id` + capture timestamp. |
| C11 | Service restart broadcasts `server_shutdown` to all connected modules. |

## 12. Build & Run

```bash
./scripts/run.sh build              # CMake build
./scripts/run.sh start              # generate certs (if needed) + start
./scripts/run.sh {stop|restart|status}
```

Manual:

```bash
cmake -S . -B build && cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
./build/sim_vision_node [config_dir]
```
