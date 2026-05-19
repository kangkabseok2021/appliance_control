# Appliance Control & HiL Edge Testbed

Two embedded systems projects in one repository — sharing a C++17 HAL pattern, asyncio Python simulation, and Docker infrastructure.

| Project | Description | Docs |
|---|---|---|
| **Washer FSM** | C++17 washing-cycle state machine + Python simulator over Unix socket + FastAPI dashboard | [docs/washer.md](docs/washer.md) |
| **HiL Edge Testbed** | C++17 MQTT control loop on ARM64 edge hardware + Python physics simulator + aiosqlite telemetry | [docs/hil.md](docs/hil.md) |

---

## Repository Layout

```
appliance_control/
├── controller/          Washer C++ CMake project (FSM + HAL + HTTP API)
├── simulator.py         Washer Python sensor simulator (asyncio, Unix socket)
├── api.py               Washer FastAPI proxy + JS dashboard
├── static/              Washer dashboard HTML
├── hil/
│   ├── controller/      HiL C++ CMake project (MQTT FSM + ARM64 toolchain)
│   ├── simulator_hil.py HiL Python physics simulator (paho-mqtt)
│   ├── telemetry.py     HiL aiosqlite µs telemetry logger
│   ├── tests/           HiL pytest suite (physics + fault injection)
│   └── mosquitto.conf   Mosquitto broker config for Docker
├── docs/
│   ├── washer.md        Washer system documentation
│   └── hil.md           HiL testbed documentation
├── Dockerfile.controller
├── Dockerfile.python
└── docker-compose.yml   All services: washer + mosquitto + HiL simulator + HiL controller
```

---

## Quick Start

```bash
# Washer system only
docker compose up simulator controller api

# HiL testbed only
docker compose up mosquitto hil-simulator hil-controller

# Everything
docker compose up
```

---

## CI

Four jobs on every push to `main`:

| Job | What it validates |
|---|---|
| `washer-cpp` | Washer C++ build + 12 GoogleTests |
| `hil-cpp` | HiL C++ build + 17 GoogleTests |
| `hil-arm64` | Cross-compile `hil_controller` for aarch64-linux-gnu (tests skipped — no QEMU) |
| `python` | All 27 pytest (9 washer + 13 HiL simulator + 5 HiL telemetry) |
