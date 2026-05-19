"""Async SQLite telemetry — logs every MQTT interaction with µs timestamps."""
from __future__ import annotations

import time
from pathlib import Path

import aiosqlite

DB_PATH = Path("data/hil_telemetry.db")


class HilTelemetry:
    def __init__(self, db_path: Path = DB_PATH) -> None:
        self._path = db_path
        self._db: aiosqlite.Connection | None = None

    async def open(self) -> None:
        self._path.parent.mkdir(parents=True, exist_ok=True)
        self._db = await aiosqlite.connect(self._path)
        await self._db.execute("PRAGMA journal_mode=WAL")
        await self._db.executescript("""
            CREATE TABLE IF NOT EXISTS interactions (
                ts_us          INTEGER PRIMARY KEY,
                direction      TEXT NOT NULL,   -- 'publish' | 'receive'
                topic          TEXT NOT NULL,
                payload_bytes  INTEGER,
                round_trip_us  INTEGER           -- NULL if no round-trip data
            );
            CREATE TABLE IF NOT EXISTS fault_events (
                ts_us          INTEGER PRIMARY KEY,
                fault_type     TEXT NOT NULL,    -- PACKET_DROP | CORRUPT | TIMEOUT
                topic          TEXT,
                detail         TEXT
            );
        """)
        await self._db.commit()

    async def close(self) -> None:
        if self._db:
            await self._db.close()

    async def log_interaction(self, direction: str, topic: str,
                               payload_bytes: int, round_trip_us: int | None = None) -> None:
        assert self._db
        await self._db.execute(
            "INSERT OR REPLACE INTO interactions VALUES (?,?,?,?,?)",
            (int(time.time() * 1_000_000), direction, topic, payload_bytes, round_trip_us),
        )
        await self._db.commit()

    async def log_fault(self, fault_type: str, topic: str = "", detail: str = "") -> None:
        assert self._db
        await self._db.execute(
            "INSERT OR REPLACE INTO fault_events VALUES (?,?,?,?)",
            (int(time.time() * 1_000_000), fault_type, topic, detail),
        )
        await self._db.commit()

    async def recent_interactions(self, limit: int = 20) -> list[dict]:
        assert self._db
        async with self._db.execute(
            "SELECT * FROM interactions ORDER BY ts_us DESC LIMIT ?", (limit,)
        ) as cur:
            cols = [d[0] for d in cur.description]
            return [dict(zip(cols, row)) async for row in cur]

    async def avg_round_trip_us(self) -> float | None:
        assert self._db
        async with self._db.execute(
            "SELECT AVG(round_trip_us) FROM interactions WHERE round_trip_us IS NOT NULL"
        ) as cur:
            row = await cur.fetchone()
            return float(row[0]) if row and row[0] is not None else None
