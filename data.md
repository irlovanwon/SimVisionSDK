# Data for Communication

## Data Categories

### Visual & Geometric Data (2D)

| Data Type | Part IDs | Payload |
|-----------|----------|---------|
| StereoImage | `left`, `right` (one ZMQ frame each) | Raw file bytes from `DataSource/StereoImage/` (JPG/PNG/TIFF as stored on disk) |
| DepthMap | `depth_map` | Raw float32 (width×height) |

### Visual & Geometric Data (3D)

| Data Type | Part ID | Payload |
|-----------|---------|---------|
| PointCloud | `point_cloud` | Raw float32×4 (x,y,z,intensity) — point count depends on `depth_mode` |
| DisparityMap | `disparity_map` | Raw float32 |
| ConfidenceMap | `confidence_map` | Raw float32 (0..1) |

### Sensor & Tracking Data

| Data Type | Part ID | Payload |
|-----------|---------|---------|
| IMU | `imu` | JSON (accel, gyro) |
| Temperature | `temperature` | JSON (celsius) |
| Magnetometer | `magnetometer` | JSON (field x,y,z) |
| Barometer | `barometer` | JSON (pressure_hpa) |

## Data Type IDs

| Value | Name | Channel String | Group |
|-------|------|---------------|-------|
| 0 | StereoImage | `stereo_image` | visual_2d |
| 1 | DepthMap | `depth_map` | visual_2d |
| 2 | PointCloud | `point_cloud` | visual_3d |
| 3 | IMU | `imu` | sensor_data |
| 4 | DisparityMap | `disparity_map` | visual_3d |
| 5 | ConfidenceMap | `confidence_map` | visual_3d |
| 6 | Temperature | `temperature` | sensor_data |
| 7 | Magnetometer | `magnetometer` | sensor_data |
| 8 | Barometer | `barometer` | sensor_data |

## Data Transfer Principles

| Principle | Implementation |
|-----------|---------------|
| **File bytes on the wire** | API 1b transfers the exact bytes stored in `DataSource/` — no encode/decode. Stereo images are published as the original JPG/PNG/TIFF file bytes; depth/3D = raw float32; sensors = JSON. |
| **Format is descriptive** | Every ZMQ `parts[]` entry carries a `format` field so the consumer knows how to interpret the bytes. |
| **Lock-free SPSC** | 3 SPSC queues pass `shared_ptr<ChannelFrame>` from CaptureEngine → DataPublisher. Drop-NEWEST when full. |
| **Zero-copy ZMQ** | `zmq_msg_init_data` wraps each payload; a `shared_ptr<DataBundle>` hint frees the buffer after send. |
| **Silent drop at HWM** | All PUB sockets use `ZMQ_DONTWAIT`; drop-NEWEST when `ZMQ_SNDHWM` reached. |
| **Drop-NEWEST** | Both SPSC and ZMQ drop the NEWEST (incoming) data when full. Stale data is actively flushed on stop/last-subscriber-leaves. |

> **Why file bytes (not raw pixels):** SimVisionSDK is a drop-in camera simulator. Keeping the on-disk format on the wire means the datasource folder is the single source of truth and no decode dependency is needed in the simulator. The consumer (StereoCamera) decodes using the `format`/`channels`/`code`/`width`/`height` metadata in the header (see below).

## API 1b — ZMQ PUB/SUB Data Frame

Each grouped message is a ZMQ **multipart** message. **Frame 0 is the JSON header** (no separate topic frame — subscribers use an empty-topic subscription `""`). Frames 1..N are binary payloads, one per entry in `parts[]`.

### ZMQ Endpoints

| Group | Default IPC Endpoint |
|-------|---------------------|
| visual_2d | `ipc:///tmp/zed_vision_visual_2d` |
| visual_3d | `ipc:///tmp/zed_vision_visual_3d` |
| sensor_data | `ipc:///tmp/zed_vision_sensor_data` |

> Endpoints are configurable (`zmq.endpoints`); defaults use `zed_vision_*` so StereoCamera's existing config connects without changes.

### Header (JSON, Frame 0)

```json
{
  "group": "visual_2d",
  "ts_sec": 1781020852,
  "ts_nsec": 888402198,
  "pair_id": 35653,
  "active": true,
  "pub_id": "sim_vision_pub_01",
  "pub_ts_sec": 1781020853,
  "pub_ts_nsec": 123456789,
  "frame_index": 35653,
  "source": "SimVisionSDK/1.0",
  "parts": [
    {"id":"left",  "size":701254, "format":"JPG", "is_encoded":true, "channels":3, "code":"RGB",  "width":1920, "height":1200},
    {"id":"right", "size":679719, "format":"JPG", "is_encoded":true, "channels":3, "code":"RGB",  "width":1920, "height":1200},
    {"id":"depth_map", "size":9216000, "format":"raw_f32"}
  ]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `group` | string | `visual_2d` / `visual_3d` / `sensor_data` |
| `ts_sec` / `ts_nsec` | int64 | Capture timestamp (set by CaptureEngine after reading the frame) |
| `pair_id` | uint64 | Unique per capture tick; same value across all 3 groups from the same snapshot |
| `active` | bool | `true` for data frames |
| `pub_id` | string | Publisher id (= `server_id`) |
| `pub_ts_sec` / `pub_ts_nsec` | int64 | Publish timestamp (set by DataPublisher) |
| `frame_index` | uint64 | Monotonic capture counter |
| `parts` | array | One entry per binary payload frame (Frames 1..N) |

### `parts[]` entry — Image

Image parts (StereoImage left/right) carry full image metadata so the consumer can decode without any out-of-band config:

| Field | Type | Description |
|-------|------|-------------|
| `id` | string | `"left"` / `"right"` |
| `size` | int | Payload size in bytes |
| `format` | string | `raw_u8` / `raw_u12` (raw) **or** `JPG` / `PNG` / `TIFF` (encoded) |
| `is_encoded` | bool | `true` for JPG/PNG/TIFF; `false` for raw_u* |
| `channels` | int | Decoded channel count (e.g. 3, 4) |
| `code` | string | Channel layout code: `RGB`, `BGRA`, `GRAY`, `RGBA`, `GRAYA` |
| `width` | int | Image width in pixels |
| `height` | int | Image height in pixels |

> **Format detection:** SimVisionSDK probes the actual file content at load time (not the extension) — JPEG SOF marker, PNG IHDR, TIFF BOI, or the synthetic `SIMRAW1` test format — and reports the decoded dimensions/channels. Consumers must read `format`: if `is_encoded=true`, the payload is compressed file bytes (decode before use); if `false`, the payload is raw pixel rows.

### `parts[]` entry — Non-image

| Part `id` | Fields | Format |
|-----------|--------|--------|
| `depth_map` | `{id, size, format}` | `raw_f32` |
| `point_cloud` | `{id, size, format}` | `raw_f32x4` |
| `disparity_map` | `{id, size, format}` | `raw_f32` |
| `confidence_map` | `{id, size, format}` | `raw_f32` |
| `imu` / `temperature` / `magnetometer` / `barometer` | `{id, size, format}` | `json` |

### Multipart layout

```
Frame 0:   JSON header (snprintf-built, see above)
Frame 1:   binary payload for parts[0]
Frame 2:   binary payload for parts[1]
...
Frame N:   binary payload for parts[N-1]
```

### Service lifecycle events

- **`server_shutdown`** — broadcast on all 3 endpoints at process exit (single JSON frame, no payload parts): `{"event":"server_shutdown","pub_id":"...","ts_sec":...,"ts_nsec":...}`. Downstream consumers use this to reset state.

## Point Cloud Depth Modes

| Depth Mode | Compute Load | Precision | Point Count (sim) |
|------------|--------------|-----------|-------------------|
| **NONE** | Minimal | N/A | 0 |
| **NEURAL_LIGHT** | Low | Moderate | 10 000 |
| **NEURAL** | Moderate | High | 40 000 |
| **NEURAL_PLUS** | High | Maximum | 80 000 |
| **PERFORMANCE** | Low | Low | 5 000 |
| **QUALITY** | Moderate | Moderate | 20 000 |
| **ULTRA** | Very High | High | 60 000 |

Set via the `depth_mode` parameter.

## Simulation Data Source

- **Stereo images**: loaded into RAM at startup from `DataSource/StereoImage/`. Filename format `{camera_id}_{pair_index}.{ext}` — e.g. `L_001.jpg`, `R_001.jpg`. Captured via **round-robin** iteration. Published as the original file bytes.
- **Other data**: synthesized deterministically (depth/3D/sensors) based on the frame index.
