# Professional Appliance Control System

A simulated embedded control unit for a professional lab washer / industrial sterilizer, demonstrating full-stack embedded software architecture from HAL to web dashboard.

**C++17** manages the washing cycle state machine through abstract hardware interfaces. A **Python asyncio** server simulates physical sensor behaviour over a Unix socket. A **FastAPI + vanilla JS** dashboard provides real-time cycle monitoring and control. The full stack runs with a single `docker compose up`.

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Browser Dashboard  (HTML / Canvas / Vanilla JS)        │
│  polls /api/state every 1s — progress bar, temp gauge   │
└───────────────────┬─────────────────────────────────────┘
                    │ HTTP :8099
┌───────────────────▼─────────────────────────────────────┐
│  FastAPI Proxy  (api.py)                                │
│  /api/state  /api/history  /api/cmd/{start|stop|reset}  │
└───────────────────┬─────────────────────────────────────┘
                    │ HTTP :8080
┌───────────────────▼─────────────────────────────────────┐
│  C++17 Cycle Controller  (appliance_controller)         │
│  7-state FSM · IHeaterControl · IPumpControl · IDoorLock│
└───────────────────┬─────────────────────────────────────┘
                    │ Unix domain socket (JSON lines)
┌───────────────────▼─────────────────────────────────────┐
│  Python Sensor Simulator  (simulator.py)                │
│  first-order lag temp model · fill/drain physics · noise│
└─────────────────────────────────────────────────────────┘
```

---

## Cycle State Machine

```
IDLE → INITIALISING → WATER_INTAKE → HEATING → PROCESSING → DRAINING → COMPLETE
                ↓           ↓            ↓           ↓
                └───────────┴────────────┴───────────┴──► ERROR
```

| State | Condition to advance | Guard |
|---|---|---|
| INITIALISING | Door locked + self-test passed | Door lock failure → ERROR |
| WATER_INTAKE | Level ≥ 8.0 L | Door open → ERROR |
| HEATING | Temp at 85 °C ± 0.5 °C for 5 s | Door open → ERROR |
| PROCESSING | Hold 30 s at temperature | Door open → ERROR |
| DRAINING | Level ≤ 0.3 L | — |
| COMPLETE | — | reset() → IDLE |
| ERROR | emergencyStop() or fault | reset() → IDLE |

---

## Project Structure

```
appliance_control/
├── controller/                 C++17 CMake project
│   ├── include/
│   │   ├── IHeaterControl.h    Abstract heater interface
│   │   ├── IPumpControl.h      Abstract pump interface
│   │   ├── IDoorLock.h         Abstract door lock interface
│   │   ├── CycleState.h        FSM state enum + stateToString()
│   │   ├── CycleController.h   FSM orchestrator
│   │   └── SimulatedComponents.h  Socket-backed HAL implementations
│   ├── src/
│   │   ├── CycleController.cpp
│   │   ├── SimulatedComponents.cpp
│   │   ├── SocketSensorReader.h   Unix socket client (background thread)
│   │   └── main.cpp            DI wiring + cpp-httplib HTTP server
│   ├── tests/
│   │   └── test_fsm.cpp        12 GoogleTest cases
│   ├── third_party/
│   │   ├── httplib.h           cpp-httplib (single header)
│   │   └── json.hpp            nlohmann/json (single header)
│   └── CMakeLists.txt
├── simulator.py                Python asyncio physics simulator
├── api.py                      FastAPI proxy + static file server
├── static/index.html           Vanilla JS dashboard
├── tests/
│   └── test_simulator.py       9 pytest cases
├── pyproject.toml              uv Python project
├── Dockerfile.controller       Multi-stage C++ build
├── Dockerfile.python           uv-based Python image
└── docker-compose.yml          3-service stack with shared socket volume
```

---

## Quick Start

### Docker (recommended — full closed-loop simulation)

```bash
docker compose up --build
```

Open `http://localhost:8099`

Three services start in order: **simulator** (Python physics) → **controller** (C++ FSM) → **api** (FastAPI + dashboard). The simulator and controller communicate over a Unix socket in a shared Docker volume.

### Local development

**C++ controller:**
```bash
cd controller
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

**Python:**
```bash
uv sync
uv run python simulator.py &      # start physics simulator
uv run uvicorn api:app --port 8099 &  # start API + dashboard
./controller/build/appliance_controller  # start C++ controller
```

Open `http://localhost:8099`

---

## Testing

**C++ — GoogleTest (12 tests):**
```bash
cd controller
cmake --build build --target test_fsm
ctest --test-dir build --output-on-failure
```

Covers: all valid FSM transitions, invalid transition guards (`start()` outside IDLE, `reset()` from IDLE), door-unlock fault injection during WATER_INTAKE and HEATING, emergency stop, full drain-to-COMPLETE cycle.

**Python — pytest (9 tests):**
```bash
uv run pytest tests/ -v
```

Covers: temperature physics bounds (first-order lag, Gaussian noise), water fill/drain model, level bounded at zero, pump command parsing, unknown command resilience, sensor frame schema.

---

## Design Decisions

**Why Unix socket over MQTT?**
Lower latency for a 100 ms cycle — no broker process or network stack involved. The shared Docker volume makes the socket path identical inside and outside containers.

**Why `IHeaterControl` / `IPumpControl` / `IDoorLock` interfaces?**
Dependency injection in `main.cpp` wires simulated HAL to the controller. Swapping in a real I2C or GPIO driver requires changing only `main.cpp` — the FSM and test suite are unaffected.

**Why `running_.exchange(true)` in `start()`?**
Prevents a race where the worker thread hasn't updated the state yet when a second `start()` call arrives, which would otherwise trigger `std::terminate` by assigning to a joinable `std::thread`.

**Why `processSecs` as a constructor parameter?**
Lets unit tests run the full PROCESSING→DRAINING→COMPLETE path in 1 second instead of 30 without patching global state.

---

## API Reference

All endpoints served by the C++ controller on `:8080` (proxied through FastAPI on `:8099`):

| Method | Path | Description |
|---|---|---|
| `GET` | `/api/state` | Current FSM state, sensor readings, elapsed time |
| `GET` | `/api/history` | Last 60 state snapshots |
| `POST` | `/api/cmd/start` | Start the washing cycle (IDLE only) |
| `POST` | `/api/cmd/stop` | Emergency stop → ERROR |
| `POST` | `/api/cmd/reset` | Return to IDLE (from COMPLETE or ERROR) |

**State response shape:**
```json
{
  "state":       "HEATING",
  "temp_c":      72.4,
  "water_l":     8.0,
  "door_locked": true,
  "elapsed_sec": 45,
  "error":       ""
}
```
