# Folder Structure

Local design folder: `~/AOC/Lib/One/SimVisionSDK`
Remote path: `/home/user/ECIDS/SimVisionSDK` on `EDGE01 / EDGE02 / EDGE03`

```
SimVisionSDK/
├── CMakeLists.txt
├── README.md
├── AGENTS.md
├── .gitignore
│
├── config/
│   └── config.json
│
├── include/sim_vision/
│   ├── api/
│   │   ├── AdminServer.h
│   │   ├── CommandHandler.h
│   │   └── SessionManager.h
│   ├── capture/
│   │   └── CaptureEngine.h
│   ├── common/
│   │   ├── Config.h
│   │   ├── Logger.h
│   │   ├── Parameter.h
│   │   ├── Response.h
│   │   └── Types.h
│   ├── data/
│   │   ├── ChannelManager.h
│   │   ├── DataPipeline.h
│   │   ├── DataPublisher.h
│   │   └── SPSCQueue.h
│   └── datasource/
│       └── SimDataSource.h
│
├── src/
│   ├── main.cpp
│   ├── api/        (AdminServer, CommandHandler, SessionManager)
│   ├── capture/    (CaptureEngine)
│   ├── common/     (Types, Logger, Parameter, Response, Config)
│   ├── data/       (SPSCQueue, ChannelManager, DataPipeline, DataPublisher)
│   └── datasource/ (SimDataSource)
│
├── tests/
│   ├── CMakeLists.txt
│   ├── test_parameter.cpp
│   ├── test_spsc_queue.cpp
│   └── test_channel_manager.cpp
│
├── scripts/
│   ├── run.sh                # start|stop|restart|status|build
│   ├── gen_certs.sh          # self-signed TLS cert
│   └── gen_test_images.py    # synthetic stereo pair generator
│
├── certs/                    # server.crt / server.key (gitignored)
├── DataSource/
│   └── StereoImage/          # L_<idx>.<ext>, R_<idx>.<ext>
│
├── log/                      # runtime logs (gitignored)
├── status/                   # runtime status (gitignored)
│
└── build/                    # generated (gitignored)
    ├── sim_vision_node       # main executable
    ├── sim_vision_tests      # gtest runner
    └── libsim_vision_lib.a   # static library
```

## Build Artifacts

| Artifact | Description |
|----------|-------------|
| `build/sim_vision_node` | Main application binary |
| `build/sim_vision_tests` | Google Test runner |
| `build/libsim_vision_lib.a` | Static library (all source compiled) |

## Dependencies

| Library | CMake Find Method | Used By |
|---------|-------------------|---------|
| nlohmann_json (≥3.2.0) | `find_package` | Config, Parameter, Commands |
| ZeroMQ (libzmq) | `pkg_check_modules` | Data API (API 1b) |
| OpenSSL | `pkg_check_modules` | TLS (API 1a) |
| Google Test | `FetchContent` | Tests |

> SimVisionSDK does **not** depend on libcurl (it is the HTTPS server) or libjpeg (raw transfer only, no encoding).
