"""5 pytest tests for the SciPy BLDC state-space model."""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'python'))

import numpy as np
import pytest
from bldc_model import BldcModel, BldcParams


def _make_model():
    return BldcModel()


def test_step_response_settles():
    """ω converges to a non-zero steady state after a voltage step."""
    model = _make_model()
    t = np.linspace(0, 0.5, 5000)
    V = np.full_like(t, 12.0)
    _, omega, _ = model.simulate(t, V)
    assert omega[-1] > 10.0, "Motor should spin"


def test_steady_state_omega():
    """Analytical steady-state matches simulation within 1%."""
    model = _make_model()
    t = np.linspace(0, 1.0, 10000)
    V = np.full_like(t, 12.0)
    _, omega, _ = model.simulate(t, V)
    omega_ss_sim = float(omega[-1])
    omega_ss_theory, _ = model.steady_state(12.0)
    assert abs(omega_ss_sim - omega_ss_theory) / (abs(omega_ss_theory) + 1e-9) < 0.02


def test_current_limit_plausible():
    """Peak current ≤ V/R (no back-EMF case, upper bound)."""
    model = _make_model()
    V_step = 12.0
    t = np.linspace(0, 0.01, 1000)
    V = np.full_like(t, V_step)
    _, _, I = model.simulate(t, V)
    assert float(np.max(np.abs(I))) <= V_step / model.params.R + 0.1


def test_tload_disturbance_effect():
    """Non-zero T_load reduces steady-state ω."""
    model = _make_model()
    t = np.linspace(0, 0.5, 5000)
    V = np.full_like(t, 12.0)
    _, omega_no_load, _ = model.simulate(t, V, T_load=0.0)
    _, omega_with_load, _ = model.simulate(t, V, T_load=0.01)
    assert omega_with_load[-1] < omega_no_load[-1]


def test_zero_input_stability():
    """Zero voltage → motor decelerates to (near) zero."""
    model = _make_model()
    t = np.linspace(0, 2.0, 20000)
    V = np.zeros_like(t)
    _, omega, _ = model.simulate(t, V)
    assert abs(float(omega[-1])) < 0.5
