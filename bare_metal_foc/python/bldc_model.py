"""SciPy state-space BLDC model.

First-order electrical + mechanical model:
  L·dI/dt = V − R·I − Ke·ω
  J·dω/dt = Kt·I − B·ω − T_load
"""
from __future__ import annotations
from dataclasses import dataclass, field
import numpy as np
from scipy import signal


@dataclass
class BldcParams:
    R:  float = 0.5        # winding resistance (Ω)
    L:  float = 1e-3       # winding inductance (H)
    Ke: float = 0.05       # back-EMF constant (V·s/rad)
    Kt: float = 0.05       # torque constant (N·m/A)
    J:  float = 1e-3       # rotor inertia (kg·m²)
    B:  float = 1e-3       # viscous friction (N·m·s/rad)


@dataclass
class BldcModel:
    params: BldcParams = field(default_factory=BldcParams)

    def _build_ss(self) -> signal.StateSpace:
        p = self.params
        # State: x = [I, ω]
        A = np.array([
            [-p.R / p.L,  -p.Ke / p.L],
            [ p.Kt / p.J, -p.B  / p.J],
        ])
        B = np.array([[1.0 / p.L], [0.0]])
        C = np.eye(2)     # observe both I and ω
        D = np.zeros((2, 1))
        return signal.StateSpace(A, B, C, D)

    def simulate(self, t: np.ndarray, V_input: np.ndarray,
                 T_load: float = 0.0) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        """Return (t, omega_rad_s, I_a_amps).
        T_load: constant load torque subtracted from ω equation (simplified).
        """
        sys = self._build_ss()
        tout, yout, _ = signal.lsim(sys, V_input, t)
        I_a   = yout[:, 0]
        omega = yout[:, 1] - T_load / self.params.B  # steady-state offset
        return tout, omega, I_a

    def steady_state(self, V: float) -> tuple[float, float]:
        """Return (omega_ss, I_ss) for a constant voltage V."""
        p = self.params
        # From dI/dt=0: I_ss = (V − Ke·ω_ss) / R
        # From dω/dt=0: ω_ss = Kt·I_ss / B
        # Solve simultaneously:
        I_ss   = V * p.B / (p.R * p.B + p.Ke * p.Kt)
        omega_ss = p.Kt * I_ss / p.B
        return omega_ss, I_ss
