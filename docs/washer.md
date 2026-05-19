# Washer FSM — Professional Appliance Control System

A simulated embedded control unit for a professional lab washer / industrial sterilizer. Demonstrates full-stack embedded software: layered HAL, seven-state safety FSM, Unix-socket IPC, async SQLite telemetry, and a live Qt-style dashboard.

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│  FastAPI + Vanilla JS Dashboard  (:8099)                │
│  Cycle progress bar · Temperature gauge · Controls     │
└──────────────────┬──────────────────────────────────────┘
                   │ HTTP
┌──────────────────▼──────────────────────────────────────┐
│  api.py  FastAPI proxy  (:8099)                         │
│  /api/state  /api/enqueue  /api/logs/*                  │
└──────────────────┬──────────────────────────────────────┘
                   │ HTTP :8080
┌──────────────────▼──────────────────────────────────────┐
│  C++17 CycleController                                  │
│  IDLE→INITIALISING→WATER_INTAKE→HEATING→               │
│  PROCESSING→DRAINING→COMPLETE / ERROR                  │
│  IHeaterControl  IPumpControl  IDoorLock (ABCs)         │
└──────────────────┬──────────────────────────────────────┘
                   │ Unix domain socket (JSON lines)
┌──────────────────▼──────────────────────────────────────┐
│  simulator.py  Python asyncio physics model             │
│  temp first-order lag · water level · door state       │
└─────────────────────────────────────────────────────────┘
```

---

## State Machine

```
IDLE → INITIALISING → WATER_INTAKE → HEATING → PROCESSING → DRAINING → COMPLETE
              ↓              ↓            ↓           ↓
              └──────────────┴────────────┴───────────┴──► ERROR
```

| State | Advance condition | Door fault |
|---|---|---|
| INITIALISING | Door lock + self-test | → ERROR |
| WATER_INTAKE | Level ≥ 8.0 L | → ERROR |
| HEATING | Temp ≥ 85 °C ± 0.5 °C for 5 s | → ERROR |
| PROCESSING | Hold 30 s | → ERROR |
| DRAINING | Level ≤ 0.3 L | — |

---

## Hardware Abstraction Layer

Three abstract interfaces — swap simulated implementations for real hardware drivers in `main.cpp` only:

```cpp
class IHeaterControl { virtual void setTarget(double °C); virtual double readTemp(); };
class IPumpControl   { virtual void startFill(); virtual void startDrain(); };
class IDoorLock      { virtual bool lock(); virtual bool isLocked(); };
```

`SimulatedHeaterControl` / `SimulatedPumpControl` / `SimulatedDoorLock` read values from `SharedSensorState`, which is updated by a background thread reading from the Unix socket.

---

## IPC — Unix Domain Socket

```
Python simulator                    C++ controller
     │                                    │
     │── JSON frame every 100ms ─────────►│  SensorFrame update
     │◄─ {"cmd":"PUMP_MODE","mode":1} ────│  command response
```

`SocketSensorReader` (background thread) maintains a persistent connection with auto-reconnect. Commands are sent back on the same connection.

---

## Build & Run

```bash
# Local
cd controller
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

uv sync
uv run python simulator.py &
uv run uvicorn api:app --port 8099
./controller/build/appliance_controller
```

```bash
# Docker
docker compose up simulator controller api
```

Open `http://localhost:8099`

---

## Testing

### C++ — GoogleTest (12/12)

```bash
ctest --test-dir controller/build --output-on-failure -V
```

| Group | Tests |
|---|---|
| FSM transitions | All 12 valid + 8 invalid guards |
| Door fault | WATER_INTAKE, HEATING fault injection |
| Emergency stop | Any state → ERROR |
| Recovery | COMPLETE / ERROR → IDLE via reset() |
| Drain cycle | DRAINING → COMPLETE end-to-end |

### Python — pytest (9/9)

```bash
uv run pytest tests/ -v
```

Signal shape, monotone envelope, Gaussian noise (KS test), attenuation zone, physical bounds, reproducibility, sensor frame schema.

---

## REST API

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/api/state` | FSM state + sensor values + elapsed time |
| `POST` | `/api/enqueue` | Push recipe to FIFO queue |
| `GET` | `/api/logs/transitions` | Recent state transitions |
| `GET` | `/health` | Liveness probe |
