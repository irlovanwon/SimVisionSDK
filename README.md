# SimVisionSDK

A C++17 camera-function **simulator** that plays the role of the camera SDK (e.g. a drop-in replacement for `ZEDVisionSDK`) in the ECIDS pipeline. It loads existing sample data into RAM, simulates the capture of stereo image, depth map, point cloud, IMU, and temperature data, exposes an **HTTPS admin API** (API 1a) for session lifecycle and parameter management, and publishes raw frame data via **ZeroMQ PUB/SUB** (API 1b) for downstream consumers such as `StereoCamera`.

> Downstream consumer: [`StereoCamera`](../StereoCamera) subscribes to SimVisionSDK over ZMQ and issues commands over HTTPS — exactly as it would to a real camera SDK.

## Highlights

- **API 1a — HTTPS Server** (Hybrid: 1 accept + N worker threads, OpenSSL TLS). All client workflow + query + channel-level commands. Per-frame metadata (`client_id`, `server_id`, `recv_ts`, `send_ts`).
- **API 1b — ZMQ PUB** (3 grouped channels, zero-copy `zmq_msg_init_data`, `ZMQ_IMMEDIATE=1`, silent drop-NEWEST at HWM).
- **Simulation data source** — stereo images loaded to RAM and iterated **round-robin**; depth/disparity/confidence/point-cloud/IMU/temperature/magnetometer/barometer synthesized deterministically.
- **3 SPSC lock-free queues** (drop-NEWEST) + `std::condition_variable` with timeout (low-power wait).
- **Subscriber-driven** capture — data only flows on channels with at least one subscriber; `activate_channel`/`deactivate_channel` for force-active control.
- **Smart handling** — service-restart broadcasts `server_shutdown` to all PUB channels; stale data (SPSC + ZMQ) flushed when the last subscriber leaves.
- Raw bytes transfer, `std::shared_ptr` internal dataflow, concatenated L+R stereo frames.

## Quick Start

```bash
./scripts/run.sh build      # CMake build
./scripts/run.sh start      # generate certs (if missing) + start
./scripts/run.sh status
./scripts/run.sh restart
./scripts/run.sh stop
```

Sample data: place stereo pairs in `DataSource/StereoImage/` as `L_<idx>.<ext>` + `R_<idx>.<ext>` (e.g. `L_001.jpg`, `R_001.jpg`). A generator is provided:

```bash
python3 scripts/gen_test_images.py 4        # generates 4 synthetic pairs
```

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Language | C++17 |
| Build | CMake 3.16+ |
| Messaging | ZeroMQ (libzmq) — IPC + TCP |
| Admin | HTTPS (OpenSSL TLS, hand-rolled HTTP/1.1) |
| Config | JSON (nlohmann-json) |
| Tests | Google Test (gtest) |
| Platform | Linux aarch64 (EDGE01/02/03), Linux x86_64 (dev) |

See the [design docs](AGENTS.md) for the full specification.
