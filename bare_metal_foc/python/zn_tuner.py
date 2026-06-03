"""Ziegler-Nichols PID auto-tuner for the BLDC state-space model."""
from __future__ import annotations
from dataclasses import dataclass
import numpy as np
from bldc_model import BldcModel


@dataclass
class PidGains:
    Kp: float
    Ki: float
    Kd: float
    dt: float
    clamp_min: float = -200.0
    clamp_max: float =  200.0


def _count_zero_crossings(signal: np.ndarray) -> int:
    """Count sign changes in the signal (sustained oscillation indicator)."""
    signs = np.sign(signal - signal.mean())
    # ignore zeros
    signs = signs[signs != 0]
    return int(np.sum(np.abs(np.diff(signs)) > 0))


def tune_pid(model: BldcModel, dt: float = 1e-3,
             duration: float = 2.0,
             V_step: float = 12.0) -> PidGains:
    """
    Bisection search for ultimate gain Ku:
      - Sweep Kp from 0.1 to 100 until sustained oscillation
        (zero-crossings in ω step-response > 3).
    Then apply Z-N rules:
      Kp = 0.6·Ku,  Ki = 2·Kp/Tu,  Kd = Kp·Tu/8
    """
    t = np.arange(0.0, duration, dt)
    V_in = np.full_like(t, V_step)

    Ku = 0.1
    Tu = 0.1   # fallback oscillation period
    found = False

    for Kp_test in np.linspace(0.1, 100.0, 500):
        # Simulate simple proportional feedback: V = Kp*(ω_ref − ω)
        omega_ref = 100.0   # rad/s setpoint
        error_signal = np.zeros_like(t)
        _, omega, _ = model.simulate(t, V_in)
        error_signal = omega_ref - omega
        V_ctrl = np.clip(Kp_test * error_signal, -200.0, 200.0)
        _, omega_ctrl, _ = model.simulate(t, V_ctrl)

        crossings = _count_zero_crossings(omega_ctrl - omega_ref)
        if crossings > 3:
            Ku = Kp_test
            # Estimate period: time / half the number of crossings
            if crossings > 0:
                Tu = 2.0 * duration / crossings
            found = True
            break

    if not found:
        Ku = 5.0   # fallback if no oscillation found in range

    Kp = 0.6 * Ku
    Ki = 2.0 * Kp / Tu
    Kd = Kp * Tu / 8.0
    return PidGains(Kp=Kp, Ki=Ki, Kd=Kd, dt=dt)
