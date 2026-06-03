/* test_park.c — 8 Unity tests for Park transform + round-trip (REQ-002) */
#include "unity.h"
#include "transforms.h"
#include <math.h>
#include <time.h>

/* setUp/tearDown defined in test_runner.c */

static q16_t f2q(float f) { return (q16_t)(f * 65536.0f); }
static float q2f(q16_t q) { return (float)q / 65536.0f; }
/* Encoder ticks from angle in radians: ticks = angle/(2π) * 65536 */
static uint16_t rad_to_ticks(float rad)
{
    float t = (rad / (2.0f * (float)M_PI)) * 65536.0f;
    if (t < 0.0f) t += 65536.0f;
    return (uint16_t)t;
}

/* 1. θ=0: identity — Id=i_alpha, Iq=i_beta */
void test_park_theta0_identity(void)
{
    FocState_t in = {0}, out = {0};
    in.i_alpha = f2q(1.0f); in.i_beta = f2q(0.5f);
    FOC_park(&in, 0, &out);
    TEST_ASSERT_FLOAT_WITHIN(4.0f/65536.0f, 1.0f, q2f(out.id));
    TEST_ASSERT_FLOAT_WITHIN(4.0f/65536.0f, 0.5f, q2f(out.iq));
}

/* 2. θ=π/2: Id = i_beta, Iq = -i_alpha */
void test_park_theta_pi_over_2(void)
{
    FocState_t in = {0}, out = {0};
    in.i_alpha = f2q(1.0f); in.i_beta = f2q(0.0f);
    FOC_park(&in, rad_to_ticks((float)M_PI / 2.0f), &out);
    TEST_ASSERT_FLOAT_WITHIN(4.0f/65536.0f,  0.0f, q2f(out.id));
    TEST_ASSERT_FLOAT_WITHIN(4.0f/65536.0f, -1.0f, q2f(out.iq));
}

/* 3. θ=π: Id=-i_alpha, Iq=-i_beta */
void test_park_theta_pi(void)
{
    FocState_t in = {0}, out = {0};
    in.i_alpha = f2q(1.0f); in.i_beta = f2q(0.5f);
    FOC_park(&in, rad_to_ticks((float)M_PI), &out);
    TEST_ASSERT_FLOAT_WITHIN(4.0f/65536.0f, -1.0f, q2f(out.id));
    TEST_ASSERT_FLOAT_WITHIN(4.0f/65536.0f, -0.5f, q2f(out.iq));
}

/* 4. θ=3π/2: Id=-i_beta, Iq=i_alpha */
void test_park_theta_3pi_over_2(void)
{
    FocState_t in = {0}, out = {0};
    in.i_alpha = f2q(0.0f); in.i_beta = f2q(1.0f);
    FOC_park(&in, rad_to_ticks(3.0f * (float)M_PI / 2.0f), &out);
    TEST_ASSERT_FLOAT_WITHIN(4.0f/65536.0f, -1.0f, q2f(out.id));
    TEST_ASSERT_FLOAT_WITHIN(4.0f/65536.0f,  0.0f, q2f(out.iq));
}

/* 5. Round-trip: Park then Inv-Park recovers i_alpha, i_beta */
void test_park_round_trip_theta_pi4(void)
{
    FocState_t in = {0}, park_out = {0}, inv_out = {0};
    in.i_alpha = f2q(0.8f); in.i_beta = f2q(0.6f);
    uint16_t ticks = rad_to_ticks((float)M_PI / 4.0f);
    FOC_park(&in, ticks, &park_out);
    park_out.vd = park_out.id; park_out.vq = park_out.iq;
    FOC_inv_park(&park_out, ticks, &inv_out);
    TEST_ASSERT_FLOAT_WITHIN(8.0f/65536.0f, 0.8f, q2f(inv_out.v_alpha));
    TEST_ASSERT_FLOAT_WITHIN(8.0f/65536.0f, 0.6f, q2f(inv_out.v_beta));
}

/* 6. Round-trip at θ=π/3 */
void test_park_round_trip_theta_pi3(void)
{
    FocState_t in = {0}, park_out = {0}, inv_out = {0};
    in.i_alpha = f2q(0.5f); in.i_beta = f2q(-0.5f);
    uint16_t ticks = rad_to_ticks((float)M_PI / 3.0f);
    FOC_park(&in, ticks, &park_out);
    park_out.vd = park_out.id; park_out.vq = park_out.iq;
    FOC_inv_park(&park_out, ticks, &inv_out);
    TEST_ASSERT_FLOAT_WITHIN(8.0f/65536.0f,  0.5f, q2f(inv_out.v_alpha));
    TEST_ASSERT_FLOAT_WITHIN(8.0f/65536.0f, -0.5f, q2f(inv_out.v_beta));
}

/* 7. FOC_reset zeroes all fields */
void test_foc_reset(void)
{
    FocState_t s;
    s.ia = 12345; s.iq = -99; s.vd = 777;
    FOC_reset(&s);
    TEST_ASSERT_EQUAL_INT(0, s.ia);
    TEST_ASSERT_EQUAL_INT(0, s.iq);
    TEST_ASSERT_EQUAL_INT(0, s.vd);
}

/* 8. REQ-002: Park transform completes within 10 µs (on CI host) */
void test_park_latency_under_10us(void)
{
    FocState_t in = {0}, out = {0};
    in.i_alpha = f2q(1.0f); in.i_beta = f2q(0.5f);
    uint16_t ticks = rad_to_ticks(0.5f);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    FOC_park(&in, ticks, &out);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    long ns = (t1.tv_sec - t0.tv_sec) * 1000000000L +
              (t1.tv_nsec - t0.tv_nsec);
    TEST_ASSERT_LESS_THAN(10000L, ns);  /* < 10 µs */
}

int run_park_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_park_theta0_identity);
    RUN_TEST(test_park_theta_pi_over_2);
    RUN_TEST(test_park_theta_pi);
    RUN_TEST(test_park_theta_3pi_over_2);
    RUN_TEST(test_park_round_trip_theta_pi4);
    RUN_TEST(test_park_round_trip_theta_pi3);
    RUN_TEST(test_foc_reset);
    RUN_TEST(test_park_latency_under_10us);
    return UNITY_END();
}
