/* test_clarke.c — 6 Unity tests for Clarke transform (REQ-001) */
#include "unity.h"
#include "transforms.h"
#include <math.h>  /* reference only — not used in production code */

/* setUp/tearDown defined in test_runner.c */

/* Helper: float → Q16.16 */
static q16_t f2q(float f) { return (q16_t)(f * 65536.0f); }
/* Helper: Q16.16 → float */
static float q2f(q16_t q) { return (float)q / 65536.0f; }

/* 1. Balanced three-phase: ia=1A, ib=ic=-0.5A → i_alpha=1, i_beta=0
   (ia + 2·ib) / sqrt(3) = (1 + 2·(-0.5)) / sqrt(3) = 0 */
void test_clarke_balanced(void)
{
    FocState_t in = {0}, out = {0};
    in.ia = f2q(1.0f); in.ib = f2q(-0.5f);
    FOC_clarke(&in, &out);
    TEST_ASSERT_FLOAT_WITHIN(2.0f/65536.0f, 1.0f, q2f(out.i_alpha));
    TEST_ASSERT_FLOAT_WITHIN(2.0f/65536.0f, 0.0f, q2f(out.i_beta));
}

/* 2. ia=0, ib=1A → i_alpha=0, i_beta=2/sqrt(3) */
void test_clarke_ia_zero(void)
{
    FocState_t in = {0}, out = {0};
    in.ia = 0; in.ib = f2q(1.0f);
    FOC_clarke(&in, &out);
    TEST_ASSERT_EQUAL_INT(0, out.i_alpha);
    TEST_ASSERT_FLOAT_WITHIN(2.0f/65536.0f, 2.0f/sqrtf(3.0f), q2f(out.i_beta));
}

/* 3. ib=0, ia=1A → i_alpha=1, i_beta=1/sqrt(3) */
void test_clarke_ib_zero(void)
{
    FocState_t in = {0}, out = {0};
    in.ia = f2q(1.0f); in.ib = 0;
    FOC_clarke(&in, &out);
    TEST_ASSERT_FLOAT_WITHIN(2.0f/65536.0f, 1.0f,           q2f(out.i_alpha));
    TEST_ASSERT_FLOAT_WITHIN(2.0f/65536.0f, 1.0f/sqrtf(3.0f), q2f(out.i_beta));
}

/* 4. All equal currents (ia=ib=1A) — not balanced but tests linearity */
void test_clarke_all_equal(void)
{
    FocState_t in = {0}, out = {0};
    in.ia = f2q(1.0f); in.ib = f2q(1.0f);
    FOC_clarke(&in, &out);
    TEST_ASSERT_FLOAT_WITHIN(2.0f/65536.0f, 1.0f,           q2f(out.i_alpha));
    TEST_ASSERT_FLOAT_WITHIN(2.0f/65536.0f, 3.0f/sqrtf(3.0f), q2f(out.i_beta));
}

/* 5. Edge: ia=1, ib=ic=-0.5 — i_alpha=ia */
void test_clarke_alpha_equals_ia(void)
{
    FocState_t in = {0}, out = {0};
    q16_t expected_ia = f2q(0.75f);
    in.ia = expected_ia; in.ib = f2q(-0.1f);
    FOC_clarke(&in, &out);
    TEST_ASSERT_EQUAL_INT(expected_ia, out.i_alpha);  /* exact: i_alpha = ia */
}

/* 6. Negative ia: ia=-1, ib=0.5 → i_alpha=-1, i_beta=(−1+1)/sqrt(3)=0 */
void test_clarke_negative_ia(void)
{
    FocState_t in = {0}, out = {0};
    in.ia = f2q(-1.0f); in.ib = f2q(0.5f);
    FOC_clarke(&in, &out);
    TEST_ASSERT_FLOAT_WITHIN(2.0f/65536.0f, -1.0f, q2f(out.i_alpha));
    TEST_ASSERT_FLOAT_WITHIN(2.0f/65536.0f,  0.0f, q2f(out.i_beta));
}

int run_clarke_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_clarke_balanced);
    RUN_TEST(test_clarke_ia_zero);
    RUN_TEST(test_clarke_ib_zero);
    RUN_TEST(test_clarke_all_equal);
    RUN_TEST(test_clarke_alpha_equals_ia);
    RUN_TEST(test_clarke_negative_ia);
    return UNITY_END();
}
