# Status

## Overview

Runtime status is queryable via `check_status` (API 1a). Returns the initialized state, sessions, per-channel activation + subscriber counts, pipeline metrics, and publisher stats.

## CheckStatus Response

```json
{
  "code": 0,
  "message": "Status",
  "data": {
    "initialized": true,
    "sessions": 1,
    "simulator": "SimVisionSDK/1.0",
    "stereo_pairs_loaded": 3,
    "frames_produced": 55,
    "channels": {
      "total_clients": 1,
      "channels": [
        {"data_type":"stereo_image","group":"visual_2d","subscribers":1,"activated":true,"force_active":false},
        {"data_type":"depth_map","group":"visual_2d","subscribers":1,"activated":true,"force_active":false},
        {"data_type":"imu","group":"sensor_data","subscribers":1,"activated":true,"force_active":false}
      ]
    },
    "publisher": {
      "sent_2d": 15,
      "sent_3d": 0,
      "sent_sensor": 15,
      "dropped_hwm": 0,
      "shutdown_sent": 0
    },
    "pipeline": {
      "pushed_2d": 15, "pushed_3d": 0, "pushed_sensor": 15,
      "dropped_2d": 0, "dropped_3d": 0, "dropped_sensor": 0,
      "queue_2d": 0, "queue_3d": 0, "queue_sensor": 0
    },
    "_frame": { "client_id":..., "server_id":"sim_vision_01", ... }
  }
}
```

## Field Reference

### Top-Level

| Field | Type | Description |
|-------|------|-------------|
| `initialized` | bool | Always true (service auto-inits) |
| `sessions` | int | Connected client sessions |
| `simulator` | string | Build/version tag |
| `stereo_pairs_loaded` | int | Stereo pairs loaded into RAM |
| `frames_produced` | uint64 | Total frames produced by CaptureEngine |
| `channels` | object | Per-channel activation/subscriber info |
| `publisher` | object | ZMQ PUB stats |
| `pipeline` | object | SPSC pipeline metrics |

### Channel Entry

| Field | Description |
|-------|-------------|
| `data_type` | Channel name |
| `group` | Group name |
| `subscribers` | Active subscriber count |
| `activated` | Producing? (subscribers > 0 \|\| force_active) |
| `force_active` | Force-activated via `activate_channel` |

### Publisher Stats

| Field | Description |
|-------|-------------|
| `sent_2d` / `sent_3d` / `sent_sensor` | Successful multipart sends per group |
| `dropped_hwm` | Frames dropped at HWM (EAGAIN) |
| `shutdown_sent` | `server_shutdown` broadcasts sent |

### Pipeline Metrics

| Field | Description |
|-------|-------------|
| `pushed_*` / `dropped_*` | SPSC push counts / drop-NEWEST counts |
| `queue_*` | Current SPSC depth per group |
