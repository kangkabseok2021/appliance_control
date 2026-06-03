# Requirements — Bare-Metal FOC Engine

V-model traceability: each REQ-XXX maps to a Unity test via `docs/VMODEL.md`.

| ID      | Requirement | Acceptance Criterion |
|---------|-------------|----------------------|
| REQ-001 | Clarke transform shall produce Iβ within 0.001 A of analytical reference for balanced three-phase input (ia + ib + ic = 0) | `test_clarke_balanced` PASS; error ≤ 2 LSB in Q16.16 |
| REQ-002 | Park transform shall compute [Id, Iq] within 10 µs on CI host (ubuntu-latest) | `test_park_latency_under_10us` PASS |
| REQ-003 | CAN packer shall encode torque in [−200, +200] Nm without data loss; overflow input clamped to ±200 | `test_can_overflow_clamp` PASS; return code = −2 on clamp |
| REQ-004 | CAN unpacker shall reconstruct torque within ±1/256 Nm (one Q8.8 LSB ≈ 0.004 Nm) | `test_can_pack_unpack_round_trip_200Nm` PASS |
| REQ-005 | All Unity assertions shall pass with zero failures | `make test` → Unity exit code 0 |
| REQ-006 | gcov statement coverage = 100% and branch coverage ≥ 90% for `src/` | `make coverage` → `check_coverage.py` PASS |
