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

See [design.md](design.md) §9 for response codes. Each response carries a `_frame` object (see [API_module.md](API_module.md)).

## Group Name Expansion

Group names are accepted in addition to individual data type names for `start_capture`, `stop_capture`, `activate_channel`, `deactivate_channel`:

| Group Name | Expanded To |
|-----------|-------------|
| `visual_2d` / `visual_geometric_2d` | StereoImage, DepthMap |
| `visual_3d` / `visual_geometric_3d` | PointCloud, DisparityMap, ConfidenceMap |
| `sensor_data` / `sensor_tracking` | IMU, Temperature, Magnetometer, Barometer |
