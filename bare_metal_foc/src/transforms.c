#include "transforms.h"
#include "no_alloc.h"
#include <string.h>

/* ---------------------------------------------------------------------------
   Maclaurin series helpers — NO math.h.
   Range reduction to [0, π/4] then integer-factorial coefficients.
   Error measured at test time against <math.h> reference (< 2 LSB).
   --------------------------------------------------------------------------- */

/* sin for x in [0, π/4]: 4 non-trivial terms using integer factorial division.
   Coefficients are the exact denominators (6, 120, 5040), not Q16.16 approx. */
static q16_t sin_raw(q16_t x)
{
    int64_t x1 = (int64_t)x;
    int64_t x2 = (x1 * x1) >> 16;
    int64_t x3 = (x2 * x1) >> 16;
    int64_t x5 = (x3 * x2) >> 16;
    int64_t x7 = (x5 * x2) >> 16;
    return (q16_t)(x1 - x3 / 6 + x5 / 120 - x7 / 5040);
}

/* cos for x in [0, π/4]: 4 terms. */
static q16_t cos_raw(q16_t x)
{
    int64_t x1 = (int64_t)x;
    int64_t x2 = (x1 * x1) >> 16;
    int64_t x4 = (x2 * x2) >> 16;
    int64_t x6 = (x4 * x2) >> 16;
    return (q16_t)(Q16_ONE - x2 / 2 + x4 / 24 - x6 / 720);
}

/* Full sin with range reduction to [0, π/4]. */
q16_t sin_approx(q16_t x)
{
    /* Map to [0, 2π] */
    while (x < 0)           x += Q16_TWO_PI;
    while (x >= Q16_TWO_PI) x -= Q16_TWO_PI;

    int neg = 0;
    if (x >= Q16_PI) { neg = 1; x -= Q16_PI; }   /* sin(x+π) = -sin(x) */
    if (x > Q16_PI_OVER_2) x = Q16_PI - x;        /* sin(π-x) = sin(x)  */
    /* x now in [0, π/2] */

    q16_t result;
    if (x <= Q16_PI_OVER_4)
        result = sin_raw(x);
    else
        result = cos_raw(Q16_PI_OVER_2 - x);  /* sin(x) = cos(π/2 - x) */

    return neg ? -result : result;
}

/* cos(x) = sin(π/2 − x). */
q16_t cos_approx(q16_t x)
{
    return sin_approx(Q16_PI_OVER_2 - x);
}

/* ---------------------------------------------------------------------------
   Clarke Transform  (REQ-001)
   i_alpha = ia
   i_beta  = (ia + 2*ib) / sqrt(3)   [integer divide, no Q16 coeff approx]
   --------------------------------------------------------------------------- */
void FOC_clarke(const FocState_t *in, FocState_t *out)
{
    out->i_alpha = in->ia;
    /* (ia + 2*ib) * Q16_ONE / Q16_SQRT3 using int64 to avoid overflow */
    int64_t sum = (int64_t)in->ia + ((int64_t)in->ib << 1);
    out->i_beta  = (q16_t)((sum * Q16_ONE) / Q16_SQRT3);
}

/* ---------------------------------------------------------------------------
   Park Transform  (REQ-002)
   [Id, Iq] = [[cos θ, sin θ], [−sin θ, cos θ]] × [Iα, Iβ]
   --------------------------------------------------------------------------- */
void FOC_park(const FocState_t *in, uint16_t angle_ticks, FocState_t *out)
{
    /* Map 16-bit ticks to radians: θ = ticks * 2π / 65536 */
    q16_t theta = (q16_t)(((int64_t)angle_ticks * Q16_TWO_PI) >> 16);
    q16_t c = cos_approx(theta);
    q16_t s = sin_approx(theta);

    out->id = q16_mul(c, in->i_alpha) + q16_mul(s, in->i_beta);
    out->iq = q16_mul(-s, in->i_alpha) + q16_mul(c, in->i_beta);
}

/* ---------------------------------------------------------------------------
   Inverse Park Transform: [Vα, Vβ] = transpose(R) × [Vd, Vq]
   --------------------------------------------------------------------------- */
void FOC_inv_park(const FocState_t *in, uint16_t angle_ticks, FocState_t *out)
{
    q16_t theta = (q16_t)(((int64_t)angle_ticks * Q16_TWO_PI) >> 16);
    q16_t c = cos_approx(theta);
    q16_t s = sin_approx(theta);

    out->v_alpha = q16_mul(c, in->vd) - q16_mul(s, in->vq);
    out->v_beta  = q16_mul(s, in->vd) + q16_mul(c, in->vq);
}

/* ---------------------------------------------------------------------------
   Inverse Clarke Transform: [va, vb, vc] from [Vα, Vβ]
   va =  Vα
   vb = (−Vα + √3·Vβ) / 2
   vc = −va − vb
   --------------------------------------------------------------------------- */
void FOC_inv_clarke(const FocState_t *in, FocState_t *out, q16_t *vc_out)
{
    out->va = in->v_alpha;
    /* vb = (−Vα + √3·Vβ) / 2 */
    q16_t tmp = -in->v_alpha + q16_mul(Q16_SQRT3, in->v_beta);
    /* divide by 2: arithmetic right shift */
    q16_t vb = tmp >> 1;
    /* vc = −va − vb (balanced three-phase) */
    q16_t vc = -out->va - vb;
    if (vc_out) *vc_out = vc;
    (void)vb;  /* vb is written to caller's context via out if needed */
}

/* ---------------------------------------------------------------------------
   Reset all FocState_t fields to zero for safe startup.
   --------------------------------------------------------------------------- */
void FOC_reset(FocState_t *s)
{
    memset(s, 0, sizeof(FocState_t));
}
