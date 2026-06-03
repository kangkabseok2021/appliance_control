#ifndef TRANSFORMS_H
#define TRANSFORMS_H

#include "foc_types.h"

/* ---------------------------------------------------------------------------
   Clarke Transform  (REQ-001)
   Balanced three-phase assumption: ia + ib + ic = 0 → no ic measurement.
   i_alpha = ia
   i_beta  = (ia + 2*ib) / sqrt(3)
   --------------------------------------------------------------------------- */
void FOC_clarke(const FocState_t *in, FocState_t *out);

/* ---------------------------------------------------------------------------
   Park Transform  (REQ-002)
   Rotates (i_alpha, i_beta) by angle theta_ticks into (id, iq).
   angle_ticks: 16-bit encoder value, 0–65535 = one full electrical revolution.
   --------------------------------------------------------------------------- */
void FOC_park(const FocState_t *in, uint16_t angle_ticks, FocState_t *out);

/* Inverse Park Transform: (vd, vq) → (v_alpha, v_beta). */
void FOC_inv_park(const FocState_t *in, uint16_t angle_ticks, FocState_t *out);

/* Inverse Clarke Transform: (v_alpha, v_beta) → (va, vb, vc duty cycles).
   Returns vc via *vc_out.  va and vb are written to out->va/vb. */
void FOC_inv_clarke(const FocState_t *in, FocState_t *out, q16_t *vc_out);

/* Zero all fields of a FocState_t for safe startup. */
void FOC_reset(FocState_t *s);

/* ---------------------------------------------------------------------------
   sin / cos approximation — ANSI C, NO math.h.
   5-term Maclaurin series with range reduction to [0, π/4].
   Integer-divide-by-factorial coefficients to preserve precision in Q16.16.
   Error < 2 LSB over [−π, π] validated against <math.h> in tests.
   --------------------------------------------------------------------------- */
q16_t sin_approx(q16_t x);
q16_t cos_approx(q16_t x);

#endif /* TRANSFORMS_H */
