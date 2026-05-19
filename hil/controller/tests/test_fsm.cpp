#include <gtest/gtest.h>
#include "HilFsm.h"

// ── Helpers ──────────────────────────────────────────────────────────────

static void fillBuf(SensorBuffer& buf, double temp = 25.0, double press = 1.0) {
    SensorFrame f;
    f.spindle_temp_c = temp;
    f.clamp_pressure = press;
    f.conveyor_pos_m = 0.0;
    f.valid = true;
    buf.update(f);
}

// ── FSM transition tests ──────────────────────────────────────────────────

TEST(FsmTest, StartsInStandby) {
    HilFsm fsm;
    EXPECT_EQ(fsm.state(), HilState::STANDBY);
}

TEST(FsmTest, StandbyToInitialisingOnFirstStep) {
    HilFsm fsm;
    SensorBuffer buf;
    fsm.step(buf);
    EXPECT_EQ(fsm.state(), HilState::INITIALISING);
}

TEST(FsmTest, InitialisingRequiresValidFrames) {
    HilFsm fsm;
    SensorBuffer empty, valid; fillBuf(valid);
    fsm.step(empty);                      // → INITIALISING
    fsm.step(empty);                      // still INITIALISING (no valid frame)
    EXPECT_EQ(fsm.state(), HilState::INITIALISING);
    // Feed valid frames to complete init
    for (int i = 0; i < 5; ++i) fsm.step(valid);
    EXPECT_EQ(fsm.state(), HilState::RUNNING);
}

TEST(FsmTest, RunningAfterEnoughValidFrames) {
    HilFsm fsm;
    SensorBuffer buf; fillBuf(buf);
    fsm.step(buf);
    for (int i = 0; i < 5; ++i) fsm.step(buf);
    EXPECT_EQ(fsm.state(), HilState::RUNNING);
}

TEST(FsmTest, HighTempCausesFault) {
    HilFsm fsm;
    SensorBuffer normal; fillBuf(normal, 25.0);
    fsm.step(normal);
    for (int i = 0; i < 5; ++i) fsm.step(normal);
    EXPECT_EQ(fsm.state(), HilState::RUNNING);

    SensorBuffer hot; fillBuf(hot, 90.0);   // > TEMP_FAULT_C = 85
    fsm.step(hot);
    EXPECT_EQ(fsm.state(), HilState::FAULT);
}

TEST(FsmTest, HighPressureCausesFault) {
    HilFsm fsm;
    SensorBuffer buf; fillBuf(buf);
    fsm.step(buf);
    for (int i = 0; i < 5; ++i) fsm.step(buf);

    SensorBuffer highPress; fillBuf(highPress, 25.0, 6.0);  // > PRESS_FAULT = 5.0
    fsm.step(highPress);
    EXPECT_EQ(fsm.state(), HilState::FAULT);
}

TEST(FsmTest, EmergencyStopFromRunning) {
    HilFsm fsm;
    SensorBuffer buf; fillBuf(buf);
    fsm.step(buf);
    for (int i = 0; i < 5; ++i) fsm.step(buf);
    fsm.emergencyStop();
    EXPECT_EQ(fsm.state(), HilState::EMERGENCY_STOP);
}

TEST(FsmTest, ResetFromFaultToStandby) {
    HilFsm fsm;
    SensorBuffer buf; fillBuf(buf);
    fsm.step(buf);
    for (int i = 0; i < 5; ++i) fsm.step(buf);
    { SensorBuffer xb; fillBuf(xb, 90.0); fsm.step(xb); };   // → FAULT
    fsm.reset();
    EXPECT_EQ(fsm.state(), HilState::STANDBY);
}

TEST(FsmTest, ResetFromEmergencyStop) {
    HilFsm fsm;
    { SensorBuffer xb; fsm.step(xb); };
    fsm.emergencyStop();
    fsm.reset();
    EXPECT_EQ(fsm.state(), HilState::STANDBY);
}

TEST(FsmTest, InvalidResetFromRunningIgnored) {
    HilFsm fsm;
    SensorBuffer buf; fillBuf(buf);
    fsm.step(buf);
    for (int i = 0; i < 5; ++i) fsm.step(buf);
    EXPECT_EQ(fsm.state(), HilState::RUNNING);
    fsm.reset();   // invalid — should be no-op
    EXPECT_EQ(fsm.state(), HilState::RUNNING);
}

TEST(FsmTest, SetpointSlowsConveyorOnHighTemp) {
    HilFsm fsm;
    SensorFrame hot, normal;
    hot.spindle_temp_c = 78.0; hot.valid = true;
    normal.spindle_temp_c = 20.0; normal.valid = true;

    auto sp_hot    = fsm.computeSetpoint(hot);
    auto sp_normal = fsm.computeSetpoint(normal);
    EXPECT_LT(sp_hot.conveyor_speed_ms, sp_normal.conveyor_speed_ms);
}

TEST(FsmTest, SetpointRequestsCoolingAbove60C) {
    HilFsm fsm;
    SensorFrame warm; warm.spindle_temp_c = 65.0; warm.valid = true;
    auto sp = fsm.computeSetpoint(warm);
    EXPECT_GT(sp.spindle_coolrate, 0.0);
}

TEST(FsmTest, TransitionHistoryRecorded) {
    HilFsm fsm;
    { SensorBuffer xb; fsm.step(xb); };   // STANDBY → INITIALISING
    auto hist = fsm.drainTransitions();
    ASSERT_EQ(hist.size(), 1u);
    EXPECT_EQ(hist[0].from, "STANDBY");
    EXPECT_EQ(hist[0].to,   "INITIALISING");
}
