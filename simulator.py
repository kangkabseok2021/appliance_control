"""
Python sensor simulator — runs a physics model of the appliance and streams
sensor frames to the C++ controller via Unix domain socket.
"""
import asyncio
import json
import math
import os
import time
import numpy as np

SOCKET_PATH = os.environ.get("SOCKET_PATH", "/tmp/appliance-sim.sock")

# Physics constants
HEAT_TAU          = 120.0   # heater first-order lag time constant (s)
AMBIENT_C         = 20.0
FILL_RATE_L_S     = 0.5     # litres per second when pump filling
DRAIN_RATE_L_S    = 0.8     # litres per second when pump draining
NOISE_SIGMA_C     = 0.2     # Gaussian noise on temperature (°C)
FRAME_INTERVAL    = 0.1     # 100 ms


class AppliancePhysics:
    def __init__(self):
        self.temp_c: float       = AMBIENT_C
        self.water_level_l: float = 0.0
        self.door_locked: bool   = True
        self.heater_target_c: float = AMBIENT_C
        self.pump_mode: int      = 0   # 1=fill, -1=drain, 0=off
        self._last_ts: float     = time.monotonic()

    def step(self) -> dict:
        now = time.monotonic()
        dt = now - self._last_ts
        self._last_ts = now

        # Temperature: first-order lag T(t) = T_target*(1 - exp(-t/τ))
        self.temp_c += (self.heater_target_c - self.temp_c) * (1 - math.exp(-dt / HEAT_TAU))
        self.temp_c += float(np.random.normal(0, NOISE_SIGMA_C))
        self.temp_c = max(AMBIENT_C, min(self.temp_c, 150.0))

        # Water level
        if self.pump_mode == 1:
            self.water_level_l = min(20.0, self.water_level_l + FILL_RATE_L_S * dt)
        elif self.pump_mode == -1:
            self.water_level_l = max(0.0, self.water_level_l - DRAIN_RATE_L_S * dt)

        return {
            "temp_c":       round(self.temp_c, 2),
            "water_level_l": round(self.water_level_l, 3),
            "door_locked":  self.door_locked,
            "ts_us":        int(time.time() * 1_000_000),
        }

    def apply_command(self, cmd: dict) -> None:
        if cmd.get("cmd") == "PUMP_MODE":
            self.pump_mode = int(cmd.get("mode", 0))


async def handle_client(reader: asyncio.StreamReader,
                        writer: asyncio.StreamWriter,
                        physics: AppliancePhysics) -> None:
    addr = writer.get_extra_info("peername", "controller")
    print(f"[sim] controller connected: {addr}")
    partial = ""
    try:
        while True:
            # Send sensor frame
            frame = physics.step()
            writer.write((json.dumps(frame) + "\n").encode())
            await writer.drain()

            # Read any commands (non-blocking)
            try:
                data = await asyncio.wait_for(reader.read(256), timeout=FRAME_INTERVAL)
                if data:
                    partial += data.decode(errors="ignore")
                    while "\n" in partial:
                        line, partial = partial.split("\n", 1)
                        try:
                            physics.apply_command(json.loads(line))
                        except json.JSONDecodeError:
                            pass
            except asyncio.TimeoutError:
                pass

    except (asyncio.IncompleteReadError, ConnectionResetError, BrokenPipeError):
        pass
    finally:
        writer.close()
        print("[sim] controller disconnected")


async def main() -> None:
    if os.path.exists(SOCKET_PATH):
        os.remove(SOCKET_PATH)

    physics = AppliancePhysics()

    server = await asyncio.start_unix_server(
        lambda r, w: handle_client(r, w, physics),
        path=SOCKET_PATH,
    )
    os.chmod(SOCKET_PATH, 0o666)
    print(f"[sim] listening on {SOCKET_PATH}")
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    asyncio.run(main())
