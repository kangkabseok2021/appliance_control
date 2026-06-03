/* test_pid.c — 6 Unity tests for PID controller */
#include "unity.h"
#include "pid.h"
#include <math.h>

/* setUp/tearDown defined in test_runner.c */

/* 1. Zero error → zero output */
void test_pid_zero_error(void)
{
    PidController_t pid;
    PID_init(&pid);
    float out = PID_update(&pid, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, out);
}

/* 2. Step setpoint: positive error → positive output */
void test_pid_step_setpoint_positive(void)
{
    PidController_t pid;
    PID_init(&pid);
    float out = PID_update(&pid, 10.0f, 0.0f);
    TEST_ASSERT_TRUE(out > 0.0f);
}

/* 3. Anti-windup: large error saturates integrator, output clamped to CLAMP_MAX */
void test_pid_antiwindup_clamp(void)
{
    PidController_t pid;
    PID_init(&pid);
    /* Drive with large error many times to saturate integrator */
    float out = 0.0f;
    for (int i = 0; i < 10000; i++)
        out = PID_update(&pid, 1000.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, PID_CLAMP_MAX, out);
}

/* 4. Integrator reset → clears accumulated state */
void test_pid_integrator_reset(void)
{
    PidController_t pid;
    PID_init(&pid);
    PID_update(&pid, 100.0f, 0.0f);
    PID_update(&pid, 100.0f, 0.0f);
    PID_reset(&pid);
    float out = PID_update(&pid, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, out);
}

/* 5. Negative error → negative (or reduced) output */
void test_pid_negative_error(void)
{
    PidController_t pid;
    PID_init(&pid);
    float out = PID_update(&pid, -10.0f, 0.0f);
    TEST_ASSERT_TRUE(out < 0.0f);
}

/* 6. Output is clamped to [CLAMP_MIN, CLAMP_MAX] */
void test_pid_output_clamped(void)
{
    PidController_t pid;
    PID_init(&pid);
    for (int i = 0; i < 1000; i++) PID_update(&pid, 9999.0f, 0.0f);
    float out_high = PID_update(&pid, 9999.0f, 0.0f);
    TEST_ASSERT_LESS_OR_EQUAL(PID_CLAMP_MAX + 0.01f, out_high);

    PID_reset(&pid);
    for (int i = 0; i < 1000; i++) PID_update(&pid, -9999.0f, 0.0f);
    float out_low = PID_update(&pid, -9999.0f, 0.0f);
    TEST_ASSERT_GREATER_OR_EQUAL(PID_CLAMP_MIN - 0.01f, out_low);
}

int run_pid_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_pid_zero_error);
    RUN_TEST(test_pid_step_setpoint_positive);
    RUN_TEST(test_pid_antiwindup_clamp);
    RUN_TEST(test_pid_integrator_reset);
    RUN_TEST(test_pid_negative_error);
    RUN_TEST(test_pid_output_clamped);
    return UNITY_END();
}
