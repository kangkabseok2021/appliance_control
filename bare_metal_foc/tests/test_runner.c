/* test_runner.c — orchestrates all Unity test suites.
   setUp/tearDown must be defined exactly once in the link unit. */
#include "unity.h"
#include <stdio.h>

/* Unity calls these before/after every individual test */
void setUp(void)    {}
void tearDown(void) {}

/* Forward declarations */
int run_clarke_tests(void);
int run_park_tests(void);
int run_pid_tests(void);
int run_can_tests(void);
int run_integration_tests(void);

int main(void)
{
    int failures = 0;
    failures += run_clarke_tests();
    failures += run_park_tests();
    failures += run_pid_tests();
    failures += run_can_tests();
    failures += run_integration_tests();
    return (failures > 0) ? 1 : 0;
}
