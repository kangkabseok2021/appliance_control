"""
FastAPI proxy — forwards commands to the C++ controller on :8080,
caches state for the JS dashboard, and serves static files.
"""
import asyncio
import json
import time
from collections import deque
from pathlib import Path

import httpx
from fastapi import FastAPI, HTTPException
from fastapi.responses import JSONResponse
from fastapi.staticfiles import StaticFiles

CONTROLLER_URL = "http://localhost:8080"
HISTORY_LEN    = 60   # keep last 60 snapshots (~60s at 1s poll)

app = FastAPI(title="Appliance Control API")

_history: deque = deque(maxlen=HISTORY_LEN)
_last_state: dict = {}


async def _fetch_state() -> dict:
    async with httpx.AsyncClient(timeout=1.0) as client:
        r = await client.get(f"{CONTROLLER_URL}/state")
        r.raise_for_status()
        return r.json()


@app.get("/api/state")
async def get_state():
    try:
        state = await _fetch_state()
        state["ts"] = time.time()
        _last_state.update(state)
        _history.append(dict(state))
        return state
    except Exception:
        if _last_state:
            return {**_last_state, "stale": True}
        raise HTTPException(status_code=503, detail="Controller unavailable")


@app.get("/api/history")
async def get_history():
    return list(_history)


@app.post("/api/cmd/{command}")
async def send_command(command: str):
    if command not in ("start", "stop", "reset"):
        raise HTTPException(status_code=400, detail="Unknown command")
    try:
        async with httpx.AsyncClient(timeout=2.0) as client:
            r = await client.post(f"{CONTROLLER_URL}/cmd/{command}")
            return JSONResponse(content=r.json(), status_code=r.status_code)
    except Exception as e:
        raise HTTPException(status_code=503, detail=str(e))


# Serve dashboard
static_dir = Path(__file__).parent / "static"
app.mount("/", StaticFiles(directory=str(static_dir), html=True), name="static")
