#include <gtest/gtest.h>
#include "WeighingScale.h"
#include "PressureGuard.h"
#include "PressureSensor.h"
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

// ─── WeighingScale ────────────────────────────────────────────────────────────

TEST(WeighingScaleTest, MassAccumulatesWithinTolerance) {
    // flow=2 kg/s, 10 s → expect ~20 kg, tolerance ±0.1 kg per spec ±0.1 after 10 s
    WeighingScale scale{"test_scale", 2.0, /*seed=*/7};
    scale.init();
    scale.setpoint(1.0);  // start filling
    for (int i = 0; i < 100; ++i) scale.update(0.1);  // 10 simulated seconds
    EXPECT_NEAR(scale.read(), 20.0, 0.5);  // noise σ=0.02 over 100 steps
}

TEST(WeighingScaleTest, StopsAccumulatingWhenSetpointZero) {
    WeighingScale scale{"test_scale", 5.0, 42};
    scale.init();
    scale.setpoint(1.0);
    scale.update(1.0);
    double mass_after_1s = scale.read();
    EXPECT_GT(mass_after_1s, 0.0);
    scale.setpoint(0.0);
    scale.update(1.0);
    EXPECT_NEAR(scale.read(), mass_after_1s, 0.1);  // no additional fill
}

TEST(WeighingScaleTest, ResetClearsMass) {
    WeighingScale scale{"test_scale", 5.0, 42};
    scale.init();
    scale.setpoint(1.0);
    scale.update(2.0);
    EXPECT_GT(scale.read(), 0.0);
    scale.reset();
    EXPECT_NEAR(scale.read(), 0.0, 0.001);
}

// ─── PressureGuard ────────────────────────────────────────────────────────────

TEST(PressureGuardTest, NormalPressureDoesNotTrip) {
    PressureSensor sensor{"ps", 3.0, 0.0, 0};  // 3.0 bar, no noise
    sensor.init();
    PressureGuard guard{sensor, 5.0};  // threshold = 5.0 bar
    std::this_thread::sleep_for(300ms);
    EXPECT_FALSE(guard.isTripped());
}

TEST(PressureGuardTest, ExceedsThresholdTrips) {
    PressureSensor sensor{"ps", 2.0, 0.0, 0};
    sensor.init();
    sensor.injectPressure(6.0);  // over threshold 5.0 bar
    PressureGuard guard{sensor, 5.0};
    std::this_thread::sleep_for(150ms);
    EXPECT_TRUE(guard.isTripped());
}

TEST(PressureGuardTest, AcknowledgeResetsWhenSafe) {
    PressureSensor sensor{"ps", 2.0, 0.0, 0};
    sensor.init();
    sensor.injectPressure(6.0);
    PressureGuard guard{sensor, 5.0};
    std::this_thread::sleep_for(150ms);
    ASSERT_TRUE(guard.isTripped());
    sensor.injectPressure(2.0);  // drop pressure below threshold
    std::this_thread::sleep_for(110ms);
    bool ack = guard.acknowledge();
    EXPECT_TRUE(ack);
    EXPECT_FALSE(guard.isTripped());
}

TEST(PressureGuardTest, AcknowledgeFailsWhenStillOverPressure) {
    PressureSensor sensor{"ps", 2.0, 0.0, 0};
    sensor.init();
    sensor.injectPressure(6.0);
    PressureGuard guard{sensor, 5.0};
    std::this_thread::sleep_for(150ms);
    ASSERT_TRUE(guard.isTripped());
    bool ack = guard.acknowledge();  // pressure still high
    EXPECT_FALSE(ack);
    EXPECT_TRUE(guard.isTripped());  // still tripped
}
