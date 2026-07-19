# Data for Communication

## Data Categories

### Visual & Geometric Data (2D)

| Data Type | Description | Payload |
|-----------|-------------|---------|
| StereoImage | Left + right images concatenated into one frame | Raw L file bytes ++ raw R file bytes |
| DepthMap | Per-pixel depth (float32, width×height) | Raw float32 |

### Visual & Geometric Data (3D)

| Data Type | Description | Payload |
|-----------|-------------|---------|
| PointCloud | 3D spatial data (float32×4: x,y,z,intensity) | Raw float32 — point count depends on `depth_mode` |
| DisparityMap | Pixel-level L/R displacement (float32) | Raw float32 |
| ConfidenceMap | Per-pixel confidence (float32, 0..1) | Raw float32 |

### Sensor & Tracking Data

| Data Type | Description | Payload |
|-----------|-------------|---------|
| IMU | Inertial measurement (accel, gyro) | JSON |
| Temperature | Camera temperature (celsius) | JSON |
| Magnetometer | Magnetic field (x,y,z) | JSON |
| Barometer | Atmospheric pressure (hPa) | JSON |

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
| **Raw bytes only** | No encoding anywhere in API 1b. Stereo image = raw L+R file bytes; depth/3D = raw float32; sensors = JSON. |
| **Lock-free SPSC** | 3 SPSC queues pass `shared_ptr<ChannelFrame>` from CaptureEngine → DataPublisher. Drop-NEWEST when full. |
| **Zero-copy ZMQ** | `zmq_msg_init_data` wraps each payload; a `shared_ptr<DataBundle>` hint frees the buffer after send. |
| **Silent drop at HWM** | All PUB sockets use `ZMQ_DONTWAIT`; drop-NEWEST when `ZMQ_SNDHWM` reached. |
| **Drop-NEWEST** | Both SPSC and ZMQ drop the NEWEST (incoming) data when full. Stale data is actively flushed on stop/last-subscriber-leaves. |

> **Rationale:** Camera data is high-bandwidth. Blocking on send would cause cascading latency — a slow consumer must never stall capture. Drop-newest preserves queued (complete) frames over newest ones that can't be sent in time.

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

- **Stereo images**: loaded into RAM at startup from `DataSource/StereoImage/`. Filename format `{camera_id}_{pair_index}.{ext}` — e.g. `L_001.jpg`, `R_001.jpg`. Captured via **round-robin** iteration.
- **Other data**: synthesized deterministically (depth/3D/sensors) based on the frame index. These are placeholders ready to be replaced with real source data.
