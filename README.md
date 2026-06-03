# Appliance Control, HiL Edge Testbed & Batch Mixer SCADA

Three embedded / industrial-control projects in one repository — all C++20, sharing a HAL pattern, asyncio Python simulation, and Docker infrastructure.

| Project | Language | Description |
|---|---|---|
| **Washer FSM** | C++20 + Python | Washing-cycle state machine + Python simulator over Unix socket + FastAPI dashboard |
| **HiL Edge Testbed** | C++20 + Python | MQTT control loop on ARM64 edge hardware + Python physics simulator + aiosqlite telemetry |
| **Batch Mixer SCADA** | C++20 | 5-state industrial FSM · `BatchComponent` Concept · `std::jthread` PressureGuard · TwinCAT ADS stub · PostgreSQL schema · Win32 Service · PowerShell scripts |

---

## Repository Layout

```
appliance_control/
├── controller/              Washer C++ CMake project (FSM + HAL + HTTP API)
├── simulator.py             Washer Python sensor simulator (asyncio, Unix socket)
├── api.py                   Washer FastAPI proxy + JS dashboard
├── static/                  Washer dashboard HTML
├── hil/
│   ├── controller/          HiL C++ CMake project (MQTT FSM + ARM64 toolchain)
│   ├── simulator_hil.py     HiL Python physics simulator (paho-mqtt)
│   ├── telemetry.py         HiL aiosqlite µs telemetry logger
│   ├── tests/               HiL pytest suite (physics + fault injection)
│   └── mosquitto.conf       Mosquitto broker config for Docker
├── batch_mixer_scada/       ── Batch Mixer SCADA ─────────────────────────────
│   ├── include/
│   │   ├── BatchComponent.h       C++20 Concept: requires init/selfTest/setpoint/read/name
│   │   ├── IBatchComponent.h      Virtual base for runtime dispatch
│   │   ├── Valve.h / WeighingScale.h / MixerDrum.h / PressureSensor.h
│   │   ├── BatchController.h      5-state FSM + recipe loading
│   │   ├── PressureGuard.h        std::jthread + std::stop_token watchdog
│   │   ├── ITwinCatLink.h         ADS interface (readCoil/writeCoil/readRegister)
│   │   ├── SimulatedAdsLink.h     In-process memory-map ADS stub
│   │   └── AdsLink.h              Real TcAdsLib stub (hardware integration guide)
│   ├── src/                 Implementations + Win32 Service entry point
│   ├── tests/
│   │   ├── test_fsm.cpp     12 FSM tests (stepUntil helper, all transitions + guards)
│   │   └── test_hal.cpp     8 HAL tests (WeighingScale accumulation, PressureGuard CR-001)
│   ├── migrations/
│   │   ├── 001_schema.sql   recipes + batch_runs + sensor_log + indexes
│   │   ├── 002_seed_recipes.sql  3 recipes: Concrete C25, Mortar M10, Grout G5
│   │   └── 003_add_pressure_threshold.sql  CR-001: idempotent ALTER + UPDATE
│   ├── scripts/
│   │   ├── Backup-BatchDB.ps1    pg_dump → zip → retain 7 days + Event Log
│   │   ├── Install-Service.ps1   New-Service + sc.exe failure recovery
│   │   └── Uninstall-Service.ps1 Stop-Service + sc delete
│   └── CMakeLists.txt       C++20, STATIC batch_mixer_lib, FetchContent GTest, Win32 exe
├── docs/
│   ├── washer.md            Washer system documentation
│   └── hil.md               HiL testbed documentation
├── Dockerfile.controller
├── Dockerfile.python
└── docker-compose.yml       Washer + mosquitto + HiL simulator + HiL controller
```

---

## Quick Start

### Washer / HiL (Docker)

```bash
docker compose up simulator controller api        # Washer only
docker compose up mosquitto hil-simulator hil-controller  # HiL only
docker compose up                                 # Everything
```

### Batch Mixer SCADA (standalone CMake)

```bash
cmake -S batch_mixer_scada -B build-batch -DCMAKE_BUILD_TYPE=Release
cmake --build build-batch --parallel
ctest --test-dir build-batch --output-on-failure -V   # 20 GoogleTests
```

**PostgreSQL setup** (optional — tests run without it):

```bash
psql -U batchmixer -d batchmixer_prod -f batch_mixer_scada/migrations/001_schema.sql
psql -U batchmixer -d batchmixer_prod -f batch_mixer_scada/migrations/002_seed_recipes.sql
```

**Windows Service install** (PowerShell, Administrator):

```powershell
.\batch_mixer_scada\scripts\Install-Service.ps1 -BinPath "C:\BatchMixer\batch_mixer_service.exe"
sc start BatchMixerSCADA
```

---

## CI

Five jobs on every push to `main`:

| Job | Runner | What it validates |
|---|---|---|
| `washer-cpp` | ubuntu-latest | Washer C++20 build + 12 GoogleTests |
| `hil-cpp` | ubuntu-latest | HiL C++20 build + 17 GoogleTests |
| `hil-arm64` | ubuntu-latest | Cross-compile `hil_controller` for aarch64-linux-gnu (C++20) |
| `batch-mixer-cpp` | ubuntu-latest | Batch Mixer C++20 build + **20 GoogleTests** (FSM transitions + PressureGuard CR-001) |
| `python` | ubuntu-latest | 27 pytest (9 washer + 13 HiL simulator + 5 HiL telemetry) |
