"""
HiL simulator unit tests — no broker needed.
Fault injection tests verify the physics model and payload handling.
"""
import json
import math
import time
import pytest
from unittest.mock import MagicMock, patch

import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).parent.parent))
from simulator_hil import PhysicsModel, HilSimulator


# ── Physics model ─────────────────────────────────────────────────────────

def test_conveyor_moves_when_speed_set():
    m = PhysicsModel()
    m.conveyor_speed = 0.5
    m.step(1.0)
    assert m.conveyor_pos_m == pytest.approx(0.5, abs=1e-6)


def test_conveyor_stationary_at_zero_speed():
    m = PhysicsModel()
    m.step(1.0)
    assert m.conveyor_pos_m == 0.0


def test_spindle_temp_rises_toward_target():
    m = PhysicsModel()
    m.spindle_temp_c = 20.0
    m.step(5.0)
    assert m.spindle_temp_c > 20.0


def test_cooling_reduces_spindle_target():
    m_hot = PhysicsModel(); m_hot.spindle_coolrate = 0.0
    m_cool = PhysicsModel(); m_cool.spindle_coolrate = 1.0
    for _ in range(10):
        m_hot.step(1.0)
        m_cool.step(1.0)
    assert m_cool.spindle_temp_c < m_hot.spindle_temp_c


def test_spindle_temp_bounded():
    m = PhysicsModel()
    m.spindle_temp_c = 200.0
    m.step(1.0)
    assert m.spindle_temp_c <= 120.0


def test_apply_command_sets_conveyor_speed():
    m = PhysicsModel()
    m.apply_command("command/actuator/conveyor", {"conveyor_speed_ms": 0.8})
    assert m.conveyor_speed == pytest.approx(0.8)


def test_apply_command_sets_coolrate():
    m = PhysicsModel()
    m.apply_command("command/actuator/conveyor", {"spindle_coolrate": 1.0})
    assert m.spindle_coolrate == pytest.approx(1.0)


def test_sensor_frames_contain_all_topics():
    m = PhysicsModel()
    frames = m.sensor_frames()
    assert "sensor/conveyor" in frames
    assert "sensor/temperature" in frames
    assert "sensor/pressure" in frames


def test_sensor_frames_contain_ts_us():
    m = PhysicsModel()
    frames = m.sensor_frames()
    for data in frames.values():
        assert "ts_us" in data
        assert isinstance(data["ts_us"], int)


# ── Fault injection ───────────────────────────────────────────────────────

class FaultInjectionTest:
    """Wraps HilSimulator._on_message for offline fault replay."""
    def __init__(self):
        self.sim = PhysicsModel()

    def inject_corrupt_payload(self, topic: str) -> None:
        """Send truncated bytes — should be silently discarded."""
        msg = MagicMock()
        msg.topic   = topic
        msg.payload = b"{\x00\xff\xfe"   # invalid UTF-8 / JSON
        # Create a minimal simulator to test the handler
        with patch("paho.mqtt.client.Client"):
            hsim = HilSimulator()
            hsim._on_message(None, None, msg)   # must not raise
            # Model unchanged — corrupt payload silently discarded
            assert hsim.model.conveyor_speed == 0.0


def test_corrupt_payload_silently_discarded():
    fi = FaultInjectionTest()
    fi.inject_corrupt_payload("command/actuator/conveyor")


def test_valid_payload_applied_after_corrupt():
    """Simulator recovers — valid message after corrupt is applied normally."""
    with patch("paho.mqtt.client.Client"):
        hsim = HilSimulator()
        # Corrupt message
        bad = MagicMock(); bad.topic = "command/actuator/conveyor"
        bad.payload = b"{bad json!!"
        hsim._on_message(None, None, bad)

        # Valid message
        good = MagicMock(); good.topic = "command/actuator/conveyor"
        good.payload = json.dumps({"conveyor_speed_ms": 0.7}).encode()
        hsim._on_message(None, None, good)

        assert hsim.model.conveyor_speed == pytest.approx(0.7)


def test_round_trip_latency_recorded():
    with patch("paho.mqtt.client.Client"):
        hsim = HilSimulator()
        ts_us = int(time.time() * 1_000_000) - 5000  # 5 ms ago
        msg = MagicMock(); msg.topic = "command/actuator/conveyor"
        msg.payload = json.dumps({
            "conveyor_speed_ms": 0.5,
            "controller_ts_us": ts_us
        }).encode()
        hsim._on_message(None, None, msg)
        lats = hsim.drain_latencies()
        assert len(lats) == 1
        assert lats[0] >= 5000.0   # at least 5 ms


def test_latency_drained_after_read():
    with patch("paho.mqtt.client.Client"):
        hsim = HilSimulator()
        msg = MagicMock(); msg.topic = "command/actuator/conveyor"
        msg.payload = json.dumps({
            "conveyor_speed_ms": 0.0,
            "controller_ts_us": int(time.time() * 1_000_000)
        }).encode()
        hsim._on_message(None, None, msg)
        hsim.drain_latencies()
        assert hsim.drain_latencies() == []
