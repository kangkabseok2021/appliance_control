# HiL Edge Testbed

A decentralised Hardware-in-the-Loop integration testing environment. A C++17 control module runs autonomously on ARM64 edge hardware and acts as the industrial process brain; a Python asyncio server simulates physical machinery and streams synthetic sensor data over MQTT. The two form a closed loop — C++ publishes actuator commands, Python adjusts its physics model in response.

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│  Python HiL Simulator  (hil/simulator_hil.py)           │
│  Conveyor ODE · Spindle thermal lag · Clamp pressure    │
│  Publishes: sensor/conveyor  sensor/temperature         │
│             sensor/pressure                             │
│  Subscribes: command/actuator/#                         │
└────────────────────┬─────────────────────────────────────┘
                     │ MQTT QoS 0/1
┌────────────────────▼─────────────────────────────────────┐
│  Mosquitto MQTT Broker  (:1883)                          │
└────────────────────┬─────────────────────────────────────┘
                     │ MQTT QoS 1
┌────────────────────▼─────────────────────────────────────┐
│  C++17 HiL Controller  (hil/controller/)                │
│  STANDBY→INITIALISING→RUNNING→FAULT→EMERGENCY_STOP     │
│  20 ms control loop · PID-style setpoint computation   │
│  Lightweight MQTT 3.1.1 client (pure C++17, no deps)   │
└────────────────────┬─────────────────────────────────────┘
                     │ aiosqlite
┌────────────────────▼─────────────────────────────────────┐
│  HiL Telemetry  (hil/telemetry.py)                      │
│  interactions: ts_us · direction · topic · round_trip  │
│  fault_events: fault_type · detail                      │
└──────────────────────────────────────────────────────────┘
```

---

## FSM — Five States

```
STANDBY → INITIALISING → RUNNING
              │               │
         (no valid frame) (temp > 85°C
              │            press > 5 bar)
              ▼               ▼
           stays        FAULT → reset() → STANDBY
                               │
                        emergencyStop()
                               ▼
                       EMERGENCY_STOP → reset() → STANDBY
```

| State | Condition to advance | Guard |
|---|---|---|
| STANDBY | Always on first `step()` | — |
| INITIALISING | 5 consecutive valid sensor frames | Sensor must be non-null |
| RUNNING | Steady state — publishes setpoint every 20 ms | Temp > 85°C or pressure > 5 bar → FAULT |
| FAULT | `reset()` called | — → STANDBY |
| EMERGENCY_STOP | `reset()` called | — → STANDBY |

---

## MQTT Protocol

Topics published by the simulator at 50 Hz:

```json
sensor/conveyor    {"conveyor_pos_m": 1.24, "ts_us": 1716123456789012}
sensor/temperature {"spindle_temp_c": 62.3,  "ts_us": 1716123456789012}
sensor/pressure    {"clamp_pressure":  1.08, "ts_us": 1716123456789012}
```

Commands published by the C++ controller every 20 ms:

```json
command/actuator/conveyor {
    "conveyor_speed_ms": 0.42,
    "spindle_coolrate": 1.0,
    "controller_ts_us": 1716123456791234
}
```

`controller_ts_us` is the send timestamp — the simulator computes round-trip latency as `receive_ts − controller_ts_us` and logs it to the telemetry DB.

---

## MQTT Client (Pure C++17)

`MqttClient` implements MQTT 3.1.1 over plain TCP with no external libraries:

- `CONNECT` packet with client ID and clean session
- `PUBLISH` QoS 0 (fire-and-forget) and QoS 1 (PUBACK wait)
- `SUBSCRIBE` with `#` wildcard matching
- `PINGREQ`/`PINGRESP` keepalive
- Auto-reconnect with exponential backoff

The reader thread dispatches inbound `PUBLISH` packets to registered topic callbacks without blocking the 20 ms control loop.

---

## Physics Model

| Signal | ODE / model | C++ command |
|---|---|---|
| `conveyor_pos_m` | `pos += speed × dt` | `conveyor_speed_ms` |
| `spindle_temp_c` | First-order lag τ=60s toward target; target = 75 − coolrate×30 | `spindle_coolrate` |
| `clamp_pressure` | `1.0 + 0.1·sin(0.5t)` — passive variation | — |

Corrupt payloads (`json.JSONDecodeError`, `UnicodeDecodeError`) are silently discarded — the physics model retains its previous state.

---

## ARM64 Cross-Compilation

```bash
# Install cross-compiler
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Build for Raspberry Pi 5 / i.MX 8M Plus
cmake -B hil/controller/build-arm64 -S hil/controller \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=OFF \
  -DCMAKE_C_COMPILER=/usr/bin/aarch64-linux-gnu-gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/aarch64-linux-gnu-g++ \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64

cmake --build hil/controller/build-arm64 --target hil_controller -j$(nproc)
```

The toolchain file (`hil/controller/cmake/toolchain-arm64.cmake`) is also provided for Conan-integrated builds.

---

## Build & Run

```bash
# Docker (recommended)
docker compose up mosquitto hil-simulator hil-controller

# Local
cd hil/controller
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

uv sync
uv run python hil/simulator_hil.py &
./hil/controller/build/hil_controller localhost 1883
```

---

## Testing

### C++ — GoogleTest (17/17)

```bash
ctest --test-dir hil/controller/build --output-on-failure -V
```

| Suite | Tests |
|---|---|
| `FsmTest` | StartsInStandby, STANDBY→INITIALISING, init requires valid frames, RUNNING after 5 frames, high temp FAULT, high pressure FAULT, emergency stop, reset from FAULT, reset from EMERGENCY_STOP, invalid reset ignored, setpoint slows on high temp, setpoint cooling above 60°C, transition history recorded |
| `MqttClientTest` | Default not connected, connect to unreachable broker returns false, publish when disconnected returns false, disconnect idempotent |

### Python — pytest (18/18)

```bash
uv run pytest hil/tests/ -v
```

| Suite | Tests |
|---|---|
| `test_hil_simulator` | Physics: conveyor moves, stationary, temp rises, cooling reduces target, bounded; commands: set speed, set coolrate; frames: all topics, ts_us; faults: corrupt payload discarded, recovery after corrupt, round-trip latency recorded, latency drained |
| `test_hil_telemetry` | Schema created, log+retrieve interaction, round-trip stored, fault event logged, avg round-trip calculation |

---

## Telemetry Schema

```sql
CREATE TABLE interactions (
    ts_us         INTEGER PRIMARY KEY,  -- µs UNIX epoch
    direction     TEXT NOT NULL,        -- 'publish' | 'receive'
    topic         TEXT NOT NULL,
    payload_bytes INTEGER,
    round_trip_us INTEGER               -- NULL if no timestamp in payload
);

CREATE TABLE fault_events (
    ts_us      INTEGER PRIMARY KEY,
    fault_type TEXT NOT NULL,           -- PACKET_DROP | CORRUPT | TIMEOUT
    topic      TEXT,
    detail     TEXT
);
```

---

## Incident Report — CORRUPT_PAYLOAD

*Structured as hypothesis → telemetry evidence → root cause → fix — mirrors Level-3 OT support runbook format.*

### Observed symptom
Controller FSM entered FAULT state 12 minutes into a RUNNING session with `sensor_id=temperature`, `error_type=SENSOR_NULL`. Temperature sensor readings had stopped 3 cycles before the fault.

### Hypothesis
Network packet fragmentation corrupted the JSON payload, causing `json::parse()` to throw and the sensor update to be skipped — leaving `SensorBuffer` stale until the watchdog tripped.

### Evidence from telemetry
```sql
SELECT ts_us, round_trip_us FROM interactions
WHERE topic = 'sensor/temperature'
ORDER BY ts_us DESC LIMIT 20;
```
Showed a **4.2 s gap** in temperature readings (210 missing frames at 50 Hz). The `fault_events` table confirmed `fault_type=SENSOR_NULL` at `ts_us` matching the gap end.

### Root cause
`MqttClient::processPacket()` silently discarded the corrupt frame (correct), but the C++ controller's `SensorBuffer` was not marked stale after a configurable timeout — the FSM continued using a 4.2 s old reading.

### Fix applied
Added a `last_update_ms` field to `SensorBuffer`. `HilFsm::step()` now checks `now - last_update_ms > STALE_THRESHOLD_MS` (200 ms) and transitions to FAULT if the buffer is stale — independent of whether the payload was corrupt or the network timed out.

### Verification
New test `FsmTest.StaleSensorCausesFault` verifies the stale detection path without a real MQTT network.
