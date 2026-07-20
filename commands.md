# Commands Reference

Shared admin commands for API 1a. See [API_module.md](API_module.md) for protocol details.

## Command Lifecycle

`init` and `dispose` are **internal lifecycle operations** — auto-executed at startup/shutdown. They are **not client-callable**:

- `init` via API → `code=3` (Already initialized).
- `dispose` via API → `code=1` (Not permitted via API).

### Client Workflow

| Command | Description | Prerequisite | Arguments |
|---------|-------------|--------------|-----------|
| `connect` | Start a client session | None | None (returns `client_id`) |
| `start_capture` | Begin capturing specified data types | `connect` | `client_id`, `data_types` |
| `stop_capture` | Stop capturing specified data types | `connect` | `client_id`, `data_types` |
| `disconnect` | End session, unsubscribe all | `connect` | `client_id` |

### Query Commands (anytime after `connect`)

| Command | Description | Prerequisite | Arguments |
|---------|-------------|--------------|-----------|
| `check_status` | System status: initialized, sessions, channels, pipeline, publisher | `connect` | None |
| `list_channels` | All channels with supported/activated flags | `connect` | None |
| `list_parameters` | All supported parameters with ranges | `connect` | None |
| `get_parameter` | Get one parameter value | `connect` | `name` |
| `set_parameter` | Set a parameter | `connect` | `name`, `value` |

### Channel-Level Control

| Command | Description | Prerequisite | Arguments |
|---------|-------------|--------------|-----------|
| `activate_channel` | Force-activate a single data type | `connect` | `data_type` |
| `deactivate_channel` | Deactivate a single data type | `connect` | `data_type` |

### Internal Lifecycle (not client-callable)

| Command | Description | When |
|---------|-------------|------|
| `init` | Initialize service | Service startup (`main.cpp`) |
| `dispose` | Dispose resources | Service shutdown (`main.cpp`) |

### Typical Client Flow

```
connect → start_capture → [subscribe ZMQ data] → stop_capture → disconnect
```

## Response

See [design.md](design.md) §9 for response codes. Every response carries the ZED-compatible envelope `{code, message, detail, server_id, client_id, recv_timestamp:{sec,nsec}, send_timestamp:{sec,nsec}}` (see [API_module.md](API_module.md)). `client_id` is a **string** supplied by the client (e.g. `"stereo_camera_1"`).

## Parameters

The simulator exposes the full ZEDVisionSDK parameter surface (so `list_parameters` / `get_parameter` work drop-in). Parameter entries carry `{name, type, value, default, min, max, is_readonly, is_available, needs_reopen, enum_options?}`.

| Parameter | Type | needs_reopen | Description |
|-----------|------|--------------|-------------|
| `fps` | Integer | true | Capture frame rate (InitParameter) |
| `resolution` | Enum | true | HD2K/HD1080/HD720/HD1200/VGA |
| `depth_mode` | Enum | true | NONE/NEURAL_LIGHT/NEURAL/NEURAL_PLUS/PERFORMANCE/QUALITY/ULTRA |
| `exposure_time` | Integer | false | Exposure (μs) |
| `gain` | Integer | false | Generic gain 0..100 (non-ZED-X) |
| `analog_gain` | Integer | false | Analog gain (ZED X series) |
| `digital_gain` | Integer | false | Digital gain (ZED X series) |
| `auto_exposure_gain` | Enum | false | On/Off |
| `mem_type` | Enum | false | CPU/GPU |
| `target_fps_2d` / `target_fps_3d` / `target_fps_sensor` | Integer | false | Timer-gate publish rate (0 = bypass) |

## Group Name Expansion

Group names are accepted in addition to individual data type names for `start_capture`, `stop_capture`, `activate_channel`, `deactivate_channel`:

| Group Name | Expanded To |
|-----------|-------------|
| `visual_2d` / `visual_geometric_2d` | StereoImage, DepthMap |
| `visual_3d` / `visual_geometric_3d` | PointCloud, DisparityMap, ConfidenceMap |
| `sensor_data` / `sensor_tracking` | IMU, Temperature, Magnetometer, Barometer |
