# Configuration

## Overview

Configuration is JSON, located in `config/config.json`, following [Coding/config_rule.md](../../Coding/config_rule.md). Loaded and validated at startup (fail-fast).

## Config Structure

```json
{
  "version": 1,
  "admin_server": {
    "host": "0.0.0.0",
    "port": 8443,
    "cert_path": "certs/server.crt",
    "key_path": "certs/server.key",
    "thread_strategy": "hybrid",
    "worker_threads": 4,
    "server_id": "sim_vision_01"
  },
  "capture": {
    "fps": 15,
    "data_source_dir": "DataSource",
    "stereo_image_dir": "DataSource/StereoImage",
    "image_width": 1280,
    "image_height": 720,
    "image_channels": 4
  },
  "zmq": {
    "transport": "ipc",
    "hwm": 10,
    "immediate": true,
    "linger_ms": 1000,
    "pub_id": "sim_vision_pub_01",
    "endpoints": {
      "visual_2d": "ipc:///tmp/sim_vision_visual_2d",
      "visual_3d": "ipc:///tmp/sim_vision_visual_3d",
      "sensor_data": "ipc:///tmp/sim_vision_sensor_data"
    }
  },
  "spsc": {
    "queue_size": 8,
    "drop_policy": "newest",
    "cv_timeout_ms": 10
  },
  "parameters": {
    "fps": 15,
    "exposure_time": 0,
    "auto_exposure": "On",
    "gain_digital": 1,
    "gain_analog": 1000,
    "auto_gain": "On",
    "depth_mode": "NEURAL"
  },
  "log_level": "info"
}
```

## Configuration Sections

### Admin Server

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `host` | string | `0.0.0.0` | HTTPS bind address |
| `port` | int | `8443` | HTTPS port |
| `cert_path` | string | `certs/server.crt` | TLS certificate |
| `key_path` | string | `certs/server.key` | TLS private key |
| `worker_threads` | int | `4` | Worker thread count (hybrid pools) |
| `server_id` | string | `sim_vision_01` | Server identity (sent in `_frame`) |

### Capture (Simulation)

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `fps` | int | `15` | Capture frame rate |
| `stereo_image_dir` | string | `DataSource/StereoImage` | Sample stereo images |
| `image_width` / `image_height` | int | `1280` / `720` | Dimensions for synthetic data |
| `image_channels` | int | `4` | Channels (BGRA) |

### ZMQ

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `transport` | string | `ipc` | `tcp` or `ipc` |
| `hwm` | int | `10` | `ZMQ_SNDHWM` — drop-NEWEST when exceeded |
| `immediate` | bool | `true` | `ZMQ_IMMEDIATE` — only queue after subscriber handshake |
| `linger_ms` | int | `1000` | `ZMQ_LINGER` |
| `pub_id` | string | `sim_vision_pub_01` | PUB identity (sent in ZMQ header) |
| `endpoints` | object | — | 3 grouped PUB endpoints (`visual_2d`, `visual_3d`, `sensor_data`) |

### SPSC

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `queue_size` | int | `8` | Max depth per SPSC queue. Drop-NEWEST when exceeded |
| `drop_policy` | string | `newest` | Drop policy (newest) |
| `cv_timeout_ms` | int | `10` | CV wait timeout for the publisher consumer thread |

### Parameters

Initial values for `ParameterManager`. All are also settable at runtime via `SetParameter`.

## Runtime Updates

`SetParameter` (API 1a) updates parameters live. `fps` and `depth_mode` carry `needs_restart=true` (semantic — a real camera would reopen; the simulator applies them at the next capture tick). All other parameters apply immediately.

## TLS Certificates

Self-signed for local/edge deployment:

```bash
./scripts/gen_certs.sh        # generates certs/server.crt + server.key
```

For production use a CA-signed certificate.
