"""
HiL Physics Simulator — publishes synthetic sensor data over MQTT
and adjusts the physics model when actuator commands arrive.

Topics published:  sensor/conveyor   sensor/temperature   sensor/pressure
Topics subscribed: command/actuator/#
"""
from __future__ import annotations

import asyncio
import json
import math
import os
import time

import paho.mqtt.client as mqtt_lib

BROKER_HOST = os.environ.get("MQTT_HOST", "localhost")
BROKER_PORT = int(os.environ.get("MQTT_PORT", "1883"))
PUBLISH_HZ  = 50.0          # sensor frames per second
DT          = 1.0 / PUBLISH_HZ


class PhysicsModel:
    """Simulated industrial machine — conveyor, spindle, clamp."""

    def __init__(self) -> None:
        self.conveyor_pos_m:  float = 0.0
        self.conveyor_speed:  float = 0.0   # m/s (set by controller command)
        self.spindle_temp_c:  float = 20.0
        self.spindle_coolrate: float = 0.0  # cooling command from controller
        self.clamp_pressure:  float = 1.0

    def step(self, dt: float) -> None:
        # Conveyor: position integrates from speed
        self.conveyor_pos_m += self.conveyor_speed * dt

        # Spindle: first-order lag toward 75°C + cooling term
        target = 75.0 - self.spindle_coolrate * 30.0
        tau    = 60.0   # thermal time constant (s)
        self.spindle_temp_c += (target - self.spindle_temp_c) * (1 - math.exp(-dt / tau))
        self.spindle_temp_c = max(15.0, min(self.spindle_temp_c, 120.0))

        # Clamp: slight sinusoidal variation
        self.clamp_pressure = 1.0 + 0.1 * math.sin(time.monotonic() * 0.5)

    def apply_command(self, topic: str, payload: dict) -> None:
        if "conveyor" in topic:
            self.conveyor_speed  = float(payload.get("conveyor_speed_ms", 0.0))
            self.spindle_coolrate = float(payload.get("spindle_coolrate", 0.0))

    def sensor_frames(self) -> dict[str, dict]:
        ts = int(time.time() * 1_000_000)
        return {
            "sensor/conveyor":     {"conveyor_pos_m": round(self.conveyor_pos_m, 4), "ts_us": ts},
            "sensor/temperature":  {"spindle_temp_c": round(self.spindle_temp_c, 2), "ts_us": ts},
            "sensor/pressure":     {"clamp_pressure":  round(self.clamp_pressure, 3), "ts_us": ts},
        }


class HilSimulator:
    def __init__(self) -> None:
        self.model   = PhysicsModel()
        self.client  = mqtt_lib.Client(mqtt_lib.CallbackAPIVersion.VERSION2,
                                       client_id="hil-simulator")
        self._loop:  asyncio.AbstractEventLoop | None = None
        self._latencies: list[float] = []

    def start(self) -> None:
        self.client.on_message = self._on_message
        self.client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)
        self.client.subscribe("command/actuator/#", qos=1)
        self.client.loop_start()

    def stop(self) -> None:
        self.client.loop_stop()
        self.client.disconnect()

    def _on_message(self, _client, _userdata, msg: mqtt_lib.MQTTMessage) -> None:
        try:
            payload = json.loads(msg.payload.decode())
            self.model.apply_command(msg.topic, payload)
            # Round-trip latency: controller embeds its send timestamp
            ctrl_ts = payload.get("controller_ts_us", 0)
            if ctrl_ts:
                now_us = int(time.time() * 1_000_000)
                self._latencies.append(float(now_us - ctrl_ts))
        except (json.JSONDecodeError, UnicodeDecodeError):
            pass   # corrupted payload — silently discarded

    def drain_latencies(self) -> list[float]:
        lats, self._latencies = self._latencies, []
        return lats

    async def run(self) -> None:
        target = time.monotonic()
        while True:
            self.model.step(DT)
            frames = self.model.sensor_frames()
            for topic, data in frames.items():
                self.client.publish(topic, json.dumps(data), qos=0)
            target += DT
            await asyncio.sleep(max(0.0, target - time.monotonic()))


async def main() -> None:
    sim = HilSimulator()
    sim.start()
    print(f"[sim] connected to {BROKER_HOST}:{BROKER_PORT}, publishing at {PUBLISH_HZ} Hz")
    try:
        await sim.run()
    finally:
        sim.stop()


if __name__ == "__main__":
    asyncio.run(main())
