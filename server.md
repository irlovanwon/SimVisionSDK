# Server Guide

| Server | IP | Role | Remote Path | Local Doc |
|--------|----|------|-------------|-----------|
| EDGE01 | 100.85.117.73 | Deployment | `/home/user/ECIDS/SimVisionSDK` | This workspace |
| EDGE02 | 100.69.131.6 | Deployment | `/home/user/ECIDS/SimVisionSDK` | This workspace |
| EDGE03 | 100.110.227.12 | Deployment | `/home/user/ECIDS/SimVisionSDK` | This workspace |

All EDGE servers: SSH, username `user`, password `admin`, Linux aarch64.

## Quick Commands

```bash
ssh user@<EDGE_IP>
cd /home/user/ECIDS/SimVisionSDK
./scripts/run.sh start        # Build (if needed) + generate certs + start
./scripts/run.sh status       # Check status
./scripts/run.sh restart      # Restart
./scripts/run.sh stop         # Graceful stop (SIGTERM, 10 s timeout)
./scripts/run.sh build        # Build only
```

## Deploy (from dev machine)

```bash
# Sync source (exclude build artifacts + runtime files)
rsync -az --delete \
  --exclude build --exclude log --exclude status --exclude '*.pid' \
  --exclude certs/*.pem --exclude certs/*.crt --exclude certs/*.key \
  /home/irlovan/AOC/Lib/One/SimVisionSDK/ \
  user@<EDGE_IP>:/home/user/ECIDS/SimVisionSDK/
```

Then on the EDGE server run `./scripts/run.sh start` (builds + generates certs locally).

## Logs & PID

| File | Description |
|------|-------------|
| `log/sim_vision_node.log` | Runtime log (timestamped, leveled) |
| `sim_vision_node.pid` | PID file (project root) |

## TLS Certificates

Self-signed certs are generated automatically on first `start` (via `scripts/gen_certs.sh`) into `certs/`. To regenerate:

```bash
./scripts/gen_certs.sh
```

## Adjacent Services (ECIDS)

| Service | Path |
|---------|------|
| StereoCamera (consumer) | `/home/user/ECIDS/StereoCamera` |
| ZEDVisionSDK (real camera) | `/home/user/ECIDS/ZEDVisionSDK` |

SimVisionSDK is a **drop-in replacement** for ZEDVisionSDK — point StereoCamera's `api1.data.channels` endpoints at SimVisionSDK's PUB endpoints to use the simulator instead of real hardware.
