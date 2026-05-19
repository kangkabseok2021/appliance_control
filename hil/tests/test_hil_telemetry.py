"""Tests for HiL aiosqlite telemetry logger."""
import pytest
from pathlib import Path

import sys
sys.path.insert(0, str(Path(__file__).parent.parent))
from telemetry import HilTelemetry


@pytest.fixture
async def tel(tmp_path):
    t = HilTelemetry(tmp_path / "test.db")
    await t.open()
    yield t
    await t.close()


@pytest.mark.asyncio
async def test_open_creates_db(tmp_path):
    t = HilTelemetry(tmp_path / "new.db")
    await t.open()
    await t.close()
    assert (tmp_path / "new.db").exists()


@pytest.mark.asyncio
async def test_log_and_retrieve_interaction(tel):
    await tel.log_interaction("publish", "sensor/conveyor", 64)
    rows = await tel.recent_interactions(5)
    assert len(rows) == 1
    assert rows[0]["topic"] == "sensor/conveyor"
    assert rows[0]["direction"] == "publish"


@pytest.mark.asyncio
async def test_round_trip_stored(tel):
    await tel.log_interaction("receive", "command/actuator/conveyor", 80, round_trip_us=3200)
    rows = await tel.recent_interactions()
    assert rows[0]["round_trip_us"] == 3200


@pytest.mark.asyncio
async def test_fault_event_logged(tel):
    await tel.log_fault("PACKET_DROP", "sensor/pressure", "1-in-5 drop")
    rows = await tel.recent_interactions()
    assert rows == []   # fault goes to separate table


@pytest.mark.asyncio
async def test_avg_round_trip(tel):
    await tel.log_interaction("receive", "cmd", 10, round_trip_us=2000)
    await tel.log_interaction("receive", "cmd", 10, round_trip_us=4000)
    avg = await tel.avg_round_trip_us()
    assert avg == pytest.approx(3000.0)
