# Lib.SimVisionSDK

## Introduction

SimVisionSDK is a C++17 service that **simulates a camera** for the ECIDS pipeline. It is a drop-in replacement for a hardware camera SDK (e.g. `ZEDVisionSDK`): it loads sample data into RAM, simulates the capturing of image, depth map, point cloud, IMU, and temperature data, exposes an **HTTPS admin API (API 1a)** for session lifecycle and parameter management, and publishes raw frame data via **ZeroMQ PUB/SUB (API 1b)** to downstream consumers.

Downstream consumer [`StereoCamera`](../StereoCamera) connects to SimVisionSDK exactly as it would to a real camera SDK.

Internal dataflow uses `std::shared_ptr` instead of memory copy to reduce allocation overhead.

## Module Architecture

| Module | Description |
|--------|-------------|
| Common | Shared types (`DataType`, `Timestamp`, `DataBundle`, `ChannelFrame`), `ParameterManager`, `Response`, `Config`, `Logger` |
| Data | `SPSCQueue` (lock-free), `DataPipeline` (3 queues + CV), `DataPublisher` (ZMQ PUB, zero-copy), `ChannelManager` (subscriber tracking) |
| DataSource | `SimDataSource` — stereo image loader (round-robin) + synthetic generators for depth/3D/sensors |
| Capture | `CaptureEngine` — FPS-paced, subscriber-driven producer thread |
| API | `AdminServer` (HTTPS hybrid pools), `CommandHandler`, `SessionManager` |

### API Overview

| API | Sub-API | From | To | Protocol | Description |
|-----|---------|------|----|----------|-------------|
| API 1a | Command | Client (StereoCamera) | SimVisionSDK | HTTPS Server (Hybrid Pools) | connect/disconnect, start/stop_capture, activate/deactivate_channel, status, parameters |
| API 1b | Data | SimVisionSDK | Client (StereoCamera) | ZMQ PUB (tcp/ipc) | Publish 3 grouped channels: visual_2d, visual_3d, sensor_data |

## Programming Language & Environment

| Item | Value |
|------|-------|
| Language | C++ 17 |
| Build System | CMake |
| Target | Linux aarch64 (EDGE01/02/03), Linux x86_64 (dev) |

## Design Principles

1. **Drop-in camera replacement** — exposes the same API surface a real camera SDK exposes.
2. **Raw transfer** — API 1b carries raw bytes only (no encoding).
3. **Shared pointers for dataflow** — `std::shared_ptr` throughout, zero-copy ZMQ via `zmq_msg_init_data`.
4. **Subscriber-driven** — data only flows on channels that have at least one subscriber.
5. **Smart handling** — service-restart notification + stale-data flush when the last subscriber leaves.

## Related Documents

- [Design](design.md) | [API Module](API_module.md) | [Commands](commands.md) | [Data](data.md)
- [Configuration](config.md) | [Thread & Data Flow](thread_dataflow.md) | [Status](status.md)
- [Folder Structure](folder_structure.md) | [Server Guide](server.md) | [Development Rules](development_rule.md)

## Server Workflow

| Server | IP | Role | Remote Path |
|--------|----|------|-------------|
| EDGE01 | 100.85.117.73 | Deployment | `/home/user/ECIDS/SimVisionSDK` |
| EDGE02 | 100.69.131.6 | Deployment | `/home/user/ECIDS/SimVisionSDK` |
| EDGE03 | 100.110.227.12 | Deployment | `/home/user/ECIDS/SimVisionSDK` |
| SERVER01 | 100.121.224.17 | Deployment | `/home/irlovan/ECIDS/SimVisionSDK` |

```bash
ssh user@<EDGE_IP>          # EDGE01/02/03 (user/admin)
ssh irlovan@<SERVER01_IP>   # SERVER01 (irlovan/123456)
cd /home/<user>/ECIDS/SimVisionSDK
./scripts/run.sh start      # Build (if needed) + start
./scripts/run.sh status     # Check status
```

## Startup Script

`scripts/run.sh {start|stop|restart|status|build}` — see [server.md](server.md).
