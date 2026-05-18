"""Tests for the Python sensor simulator physics and API."""
import asyncio
import json
import math
import pytest
from unittest.mock import AsyncMock, patch

from simulator import AppliancePhysics, AMBIENT_C, NOISE_SIGMA_C


def test_initial_temp_is_ambient():
    p = AppliancePhysics()
    assert p.temp_c == AMBIENT_C


def test_temperature_rises_toward_target():
    p = AppliancePhysics()
    p.heater_target_c = 85.0
    # Step physics manually without noise
    import time
    p._last_ts = time.monotonic() - 10  # pretend 10s passed
    original = p.temp_c
    frame = p.step()
    assert frame["temp_c"] > original, "Temperature should rise toward target"


def test_temperature_bounded():
    """Even with noise, temperature stays within physical bounds."""
    p = AppliancePhysics()
    p.heater_target_c = 90.0
    import time
    for _ in range(200):
        p._last_ts = time.monotonic() - 1
        frame = p.step()
        assert 0 < frame["temp_c"] < 155, f"Temperature {frame['temp_c']} out of bounds"


def test_water_fills_when_pump_on():
    p = AppliancePhysics()
    p.pump_mode = 1
    import time
    p._last_ts = time.monotonic() - 2  # 2 seconds of filling
    frame = p.step()
    assert frame["water_level_l"] > 0.5, "Water should have increased"


def test_water_drains_when_pump_draining():
    p = AppliancePhysics()
    p.water_level_l = 8.0
    p.pump_mode = -1
    import time
    p._last_ts = time.monotonic() - 2
    frame = p.step()
    assert frame["water_level_l"] < 8.0, "Water should have decreased"


def test_water_level_bounded_below_zero():
    p = AppliancePhysics()
    p.water_level_l = 0.0
    p.pump_mode = -1
    import time
    p._last_ts = time.monotonic() - 100
    frame = p.step()
    assert frame["water_level_l"] >= 0.0, "Water level must not go negative"


def test_pump_command_applied():
    p = AppliancePhysics()
    p.apply_command({"cmd": "PUMP_MODE", "mode": 1})
    assert p.pump_mode == 1
    p.apply_command({"cmd": "PUMP_MODE", "mode": -1})
    assert p.pump_mode == -1
    p.apply_command({"cmd": "PUMP_MODE", "mode": 0})
    assert p.pump_mode == 0


def test_unknown_command_is_ignored():
    p = AppliancePhysics()
    p.apply_command({"cmd": "UNKNOWN", "value": 99})
    # Should not raise


def test_frame_schema():
    p = AppliancePhysics()
    import time; p._last_ts = time.monotonic() - 0.1
    frame = p.step()
    assert "temp_c" in frame
    assert "water_level_l" in frame
    assert "door_locked" in frame
    assert "ts_us" in frame
    assert isinstance(frame["door_locked"], bool)
    assert isinstance(frame["ts_us"], int)
