#ifndef FOC_TYPES_H
#define FOC_TYPES_H

#include <stdint.h>
#include <assert.h>

/* Q16.16 fixed-point type: 16 integer bits, 16 fractional bits.
   Represents range roughly [-32768.0, +32768.0] with resolution 1/65536 ≈ 1.5e-5. */
typedef int32_t q16_t;

#define Q16_ONE        ((q16_t)65536)   /* 1.0  in Q16.16 */
#define Q16_SQRT3      ((q16_t)113512)  /* sqrt(3) * 65536 */
#define Q16_PI         ((q16_t)205887)  /* π    * 65536   */
#define Q16_PI_OVER_2  ((q16_t)102944)  /* π/2  * 65536   */
#define Q16_PI_OVER_4  ((q16_t)51472)   /* π/4  * 65536   */
#define Q16_TWO_PI     ((q16_t)411775)  /* 2π   * 65536   */

/* Multiply two Q16.16 values → Q16.16.  Uses int64 to prevent overflow. */
static inline q16_t q16_mul(q16_t a, q16_t b)
{
    return (q16_t)(((int64_t)a * (int64_t)b) >> 16);
}

/* Clamp a to [lo, hi]. */
static inline q16_t q16_clamp(q16_t a, q16_t lo, q16_t hi)
{
    if (a < lo) return lo;
    if (a > hi) return hi;
    return a;
}

/* FOC state: all currents and voltages in Q16.16 (Amps or Volts).
   Single global instance — zero dynamic allocation. */
typedef struct {
    q16_t ia;       /* Phase-A current      */
    q16_t ib;       /* Phase-B current      */
    q16_t i_alpha;  /* Clarke α             */
    q16_t i_beta;   /* Clarke β             */
    q16_t id;       /* d-axis current (Park)*/
    q16_t iq;       /* q-axis current (Park)*/
    q16_t vd;       /* d-axis voltage       */
    q16_t vq;       /* q-axis voltage       */
    q16_t v_alpha;  /* Inv-Park α           */
    q16_t v_beta;   /* Inv-Park β           */
    q16_t va;       /* Phase-A duty [0..1]  */
    /* ABI guard — update if struct changes */
} FocState_t;

static_assert(sizeof(FocState_t) == 44, "FocState_t size drift — update ABI guard");

#endif /* FOC_TYPES_H */
