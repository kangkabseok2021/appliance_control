/* test_integration.c — one end-to-end FOC cycle: currents → CAN frame */
#define CAN_SIMULATION
/* setUp/tearDown defined in test_runner.c */
#include "unity.h"
#include "transforms.h"
#include "pid.h"
#include "can_frame.h"
#include <math.h>


/* Full FOC cycle:
   ia=1.0, ib=-0.5 (balanced, ic=-0.5) at θ=π/4
   → Clarke → Park → PID → Inv-Park+Clarke → CAN pack → unpack
   Validates all four phases interlock correctly. */
void test_full_foc_cycle(void)
{
    static FocState_t state;
    FOC_reset(&state);

    /* ADC inputs */
    state.ia = (q16_t)(1.0f * 65536.0f);
    state.ib = (q16_t)(-0.5f * 65536.0f);

    /* Clarke */
    FocState_t c_out = {0};
    FOC_clarke(&state, &c_out);
    /* i_alpha = 1.0, i_beta = (1 + 2*(-0.5))/sqrt(3) = 0/sqrt(3) = 0 */
    TEST_ASSERT_FLOAT_WITHIN(4.0f/65536.0f, 1.0f, (float)c_out.i_alpha / 65536.0f);
    TEST_ASSERT_FLOAT_WITHIN(4.0f/65536.0f, 0.0f, (float)c_out.i_beta  / 65536.0f);

    /* Park at θ = π/4 */
    /* θ = π/4: one eighth of a full revolution → ticks = 65536/8 = 8192 */
    uint16_t ticks = 8192U;
    FocState_t p_out = {0};
    FOC_park(&c_out, ticks, &p_out);
    /* Id = cos(π/4)*1 + sin(π/4)*0 = 1/sqrt(2) */
    TEST_ASSERT_FLOAT_WITHIN(4.0f/65536.0f,
        1.0f / sqrtf(2.0f), (float)p_out.id / 65536.0f);

    /* PID on Iq */
    PidController_t pid;
    PID_init(&pid);
    float vq_float = PID_update(&pid, 0.0f, (float)p_out.iq / 65536.0f);
    p_out.vd = 0;
    p_out.vq = (q16_t)(vq_float * 65536.0f);

    /* Inv-Park */
    FocState_t ip_out = {0};
    FOC_inv_park(&p_out, ticks, &ip_out);

    /* Inv-Clarke */
    q16_t vc;
    FOC_inv_clarke(&ip_out, &ip_out, &vc);

    /* Pack torque (use Iq voltage as proxy), SoC=80%, RPM=1500 */
    CanFrame_t frame = {0};
    int ret = CanFrame_pack(vq_float, 80, 1500, &frame);
    TEST_ASSERT_TRUE(ret == 0 || ret == -2);  /* 0=ok, -2=clamped */

    /* Unpack and verify round-trip */
    float torque_rt; uint8_t soc_rt; uint16_t rpm_rt;
    TEST_ASSERT_EQUAL_INT(0, CanFrame_unpack(&frame, &torque_rt, &soc_rt, &rpm_rt));
    TEST_ASSERT_EQUAL_UINT8(80, soc_rt);
    TEST_ASSERT_EQUAL_UINT16(1500, rpm_rt);
    /* Torque round-trip within 1 Q8.8 LSB */
    float clamped = vq_float;
    if (clamped > 200.0f) clamped = 200.0f;
    if (clamped < -200.0f) clamped = -200.0f;
    TEST_ASSERT_FLOAT_WITHIN(1.0f/256.0f, clamped, torque_rt);
}

int run_integration_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_full_foc_cycle);
    return UNITY_END();
}
