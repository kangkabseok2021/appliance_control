#include <gtest/gtest.h>
#include "BatchController.h"
#include "SimulatedAdsLink.h"
#include <chrono>
#include <thread>

// ─── helpers ─────────────────────────────────────────────────────────────────

static RecipeConfig testRecipe() {
    return RecipeConfig{
        .id             = 1,
        .name           = "TestMix",
        .material_a_kg  = 5.0,
        .material_b_kg  = 3.0,
        .mix_duration_s = 2,
        .target_rpm     = 60,
        .max_pressure_bar = 6.0,
    };
}

struct ControllerFixture {
    Valve          silo_a{"silo_a"}, silo_b{"silo_b"}, discharge{"discharge"};
    WeighingScale  scale_a{"scale_a", 10.0, 1}, scale_b{"scale_b", 10.0, 2};
    MixerDrum      drum{"drum"};
    PressureSensor pressure{"pressure", 2.0, 0.0, 0};  // zero noise for determinism
    SimulatedAdsLink ads;
    BatchController ctrl{silo_a, silo_b, discharge, scale_a, scale_b, drum, pressure, ads};

    ControllerFixture() { ctrl.loadRecipe(testRecipe()); }
};

// Steps FSM until it reaches `target` state (or hits max_steps).
static BatchState stepUntil(BatchController& ctrl, BatchState target,
                             int max_steps = 100, double dt = 0.1) {
    for (int i = 0; i < max_steps; ++i) {
        auto s = ctrl.step(dt);
        if (s == target) return s;
    }
    return ctrl.state();
}

// ─── FSM tests ────────────────────────────────────────────────────────────────

TEST(FsmTest, StartsInIdle) {
    ControllerFixture f;
    EXPECT_EQ(f.ctrl.state(), BatchState::IDLE);
}

TEST(FsmTest, StartTransitionsToSiloDrain) {
    ControllerFixture f;
    EXPECT_TRUE(f.ctrl.start());
    EXPECT_EQ(f.ctrl.state(), BatchState::SILO_DRAIN);
}

TEST(FsmTest, SelfTestFailureBlocksStart) {
    Valve          silo_a{"silo_a", /*inject_leak=*/true};  // selfTest returns false
    Valve          silo_b{"silo_b"}, discharge{"discharge"};
    WeighingScale  scale_a{"scale_a"}, scale_b{"scale_b"};
    MixerDrum      drum{"drum"};
    PressureSensor pressure{"pressure"};
    SimulatedAdsLink ads;
    BatchController ctrl{silo_a, silo_b, discharge, scale_a, scale_b, drum, pressure, ads};
    ctrl.loadRecipe(testRecipe());
    EXPECT_FALSE(ctrl.start());
    EXPECT_EQ(ctrl.state(), BatchState::IDLE);
}

TEST(FsmTest, SiloDrainAdvancesToScaleFill) {
    ControllerFixture f;
    f.ctrl.start();
    // flow=10 kg/s, target_a=5 kg → reaches threshold ~5×dt=0.1 steps
    auto s = stepUntil(f.ctrl, BatchState::SCALE_FILL);
    EXPECT_EQ(s, BatchState::SCALE_FILL);
}

TEST(FsmTest, ScaleFillAdvancesToMixing) {
    ControllerFixture f;
    f.ctrl.start();
    stepUntil(f.ctrl, BatchState::SCALE_FILL);
    auto s = stepUntil(f.ctrl, BatchState::MIXING);
    EXPECT_EQ(s, BatchState::MIXING);
}

TEST(FsmTest, MixingAdvancesToDischarge) {
    ControllerFixture f;
    f.ctrl.start();
    stepUntil(f.ctrl, BatchState::MIXING);
    ASSERT_EQ(f.ctrl.state(), BatchState::MIXING);
    // mix_duration_s=2; step beyond 2 s in MIXING
    auto s = stepUntil(f.ctrl, BatchState::DISCHARGE);
    EXPECT_EQ(s, BatchState::DISCHARGE);
}

TEST(FsmTest, DischargeCompletesToIdle) {
    ControllerFixture f;
    f.ctrl.start();
    stepUntil(f.ctrl, BatchState::DISCHARGE);
    auto s = stepUntil(f.ctrl, BatchState::IDLE);
    EXPECT_EQ(s, BatchState::IDLE);
}

TEST(FsmTest, PressureTripFromMixingGoesToEStop) {
    ControllerFixture f;
    f.ctrl.start();
    stepUntil(f.ctrl, BatchState::MIXING);
    ASSERT_EQ(f.ctrl.state(), BatchState::MIXING);
    f.pressure.injectPressure(10.0);  // over threshold 6.0 bar
    // Wait for jthread poll (≤ 150 ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    f.ctrl.step(0.01);
    EXPECT_EQ(f.ctrl.state(), BatchState::EMERGENCY_STOP);
}

TEST(FsmTest, EmergencyStopLatches) {
    ControllerFixture f;
    f.ctrl.start();
    f.pressure.injectPressure(10.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    f.ctrl.step(0.01);
    ASSERT_EQ(f.ctrl.state(), BatchState::EMERGENCY_STOP);
    f.ctrl.step(0.01);
    EXPECT_EQ(f.ctrl.state(), BatchState::EMERGENCY_STOP);
}

TEST(FsmTest, AcknowledgeOnlyWorksFromEStop) {
    ControllerFixture f;
    f.ctrl.start();
    ASSERT_EQ(f.ctrl.state(), BatchState::SILO_DRAIN);
    f.ctrl.acknowledge();  // ignored — not in E_STOP
    EXPECT_EQ(f.ctrl.state(), BatchState::SILO_DRAIN);
}

TEST(FsmTest, AcknowledgeFromEStopReturnsToIdle) {
    ControllerFixture f;
    f.ctrl.start();
    f.pressure.injectPressure(10.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    f.ctrl.step(0.01);
    ASSERT_EQ(f.ctrl.state(), BatchState::EMERGENCY_STOP);
    f.pressure.injectPressure(1.0);   // drop below threshold
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    f.ctrl.acknowledge();
    EXPECT_EQ(f.ctrl.state(), BatchState::IDLE);
}

// ─── ADS link tests ───────────────────────────────────────────────────────────

TEST(AdsTest, ReadWriteCoil) {
    SimulatedAdsLink ads;
    EXPECT_FALSE(ads.readCoil(0));
    ads.writeCoil(0, true);
    EXPECT_TRUE(ads.readCoil(0));
    ads.writeCoil(0, false);
    EXPECT_FALSE(ads.readCoil(0));
}

TEST(AdsTest, ReadWriteRegister) {
    SimulatedAdsLink ads;
    EXPECT_EQ(ads.readRegister(5), 0);
    ads.writeRegister(5, 1234);
    EXPECT_EQ(ads.readRegister(5), 1234);
}
