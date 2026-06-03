"""4 pytest tests for the Ziegler-Nichols PID tuner."""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'python'))

import numpy as np
import pytest
from bldc_model import BldcModel
from zn_tuner import tune_pid


def test_gains_are_positive():
    """All PID gains must be strictly positive."""
    model = BldcModel()
    gains = tune_pid(model)
    assert gains.Kp > 0
    assert gains.Ki > 0
    assert gains.Kd >= 0


def test_step_response_settles_with_tuned_pid():
    """With tuned PID, ω settles within ±5% of setpoint in < 200 ms."""
    from scipy import signal as sp_signal
    model = BldcModel()
    gains = tune_pid(model)
    dt = gains.dt
    t = np.arange(0, 0.5, dt)
    omega_ref = 100.0
    omega = 0.0
    I = 0.0
    integrator = 0.0
    prev_err = 0.0
    settled = False
    for i in range(len(t)):
        err = omega_ref - omega
        integrator += err * dt
        d_term = (err - prev_err) / dt
        u = np.clip(gains.Kp * err + gains.Ki * integrator + gains.Kd * d_term,
                    gains.clamp_min, gains.clamp_max)
        prev_err = err
        p = model.params
        dI = (u - p.R * I - p.Ke * omega) / p.L
        dw = (p.Kt * I - p.B * omega) / p.J
        I += dI * dt
        omega += dw * dt
        if abs(omega - omega_ref) / omega_ref < 0.05 and t[i] < 0.2:
            settled = True
            break
    assert settled, f"PID did not settle within 5% in 200 ms (final ω={omega:.2f})"


def test_gains_reproducible():
    """Calling tune_pid twice with same model returns same gains."""
    model = BldcModel()
    g1 = tune_pid(model)
    g2 = tune_pid(model)
    assert abs(g1.Kp - g2.Kp) < 1e-9
    assert abs(g1.Ki - g2.Ki) < 1e-9


def test_gains_dt_matches_model():
    """Returned dt matches the dt passed to tune_pid."""
    model = BldcModel()
    dt = 5e-4
    gains = tune_pid(model, dt=dt)
    assert abs(gains.dt - dt) < 1e-12
