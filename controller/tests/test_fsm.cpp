#include <gtest/gtest.h>
#include "CycleController.h"
#include "SimulatedComponents.h"
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

// ── Stub HAL for deterministic FSM testing (no socket needed) ─────────────

class StubHeater : public IHeaterControl {
public:
    void   setTarget(double c) override { target_ = c; }
    double readTemp()   const override  { return temp_; }
    bool   isAtTarget() const override  { return atTarget_; }
    void   setTemp(double t)     { temp_ = t; }
    void   setAtTarget(bool v)   { atTarget_ = v; }
    double getTarget() const     { return target_; }
private:
    double target_    = 0.0;
    double temp_      = 20.0;
    bool   atTarget_  = false;
};

class StubPump : public IPumpControl {
public:
    void   startFill()  override { mode_ = 1; }
    void   startDrain() override { mode_ = -1; }
    void   stop()       override { mode_ = 0; }
    double readLevel()  const override { return level_; }
    void   setLevel(double l) { level_ = l; }
    int    mode() const { return mode_; }
private:
    double level_ = 0.0;
    int    mode_  = 0;
};

class StubDoor : public IDoorLock {
public:
    bool lock()     override { locked_ = true;  return true; }
    bool unlock()   override { locked_ = false; return true; }
    bool isLocked() const override { return locked_; }
    void forceUnlock() { locked_ = false; }
private:
    bool locked_ = false;
};

// ── Helper: wait for a state with timeout ──────────────────────────────────

static bool waitFor(CycleController& ctrl, CycleState expected,
                    std::chrono::milliseconds timeout = 2000ms) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (ctrl.state() == expected) return true;
        std::this_thread::sleep_for(20ms);
    }
    return false;
}

// ── Tests ──────────────────────────────────────────────────────────────────

TEST(FsmTest, StartsInIdle) {
    StubHeater h; StubPump p; StubDoor d;
    CycleController ctrl(h, p, d);
    EXPECT_EQ(ctrl.state(), CycleState::IDLE);
}

TEST(FsmTest, StartTransitionsToInitialising) {
    StubHeater h; StubPump p; StubDoor d;
    // Pre-fill so it won't block forever in WATER_INTAKE
    p.setLevel(CycleController::FILL_TARGET_L);
    h.setAtTarget(true);
    CycleController ctrl(h, p, d);
    ctrl.start();
    EXPECT_TRUE(waitFor(ctrl, CycleState::INITIALISING));
}

TEST(FsmTest, InvalidStartOutsideIdleThrows) {
    StubHeater h; StubPump p; StubDoor d;
    p.setLevel(CycleController::FILL_TARGET_L);
    h.setAtTarget(true);
    CycleController ctrl(h, p, d, 1);
    ctrl.start();
    // Wait until worker thread has set state to INITIALISING before second call
    EXPECT_TRUE(waitFor(ctrl, CycleState::INITIALISING, 500ms));
    EXPECT_THROW(ctrl.start(), std::logic_error);
}

TEST(FsmTest, InvalidResetFromIdleThrows) {
    StubHeater h; StubPump p; StubDoor d;
    CycleController ctrl(h, p, d);
    EXPECT_THROW(ctrl.reset(), std::logic_error);
}

TEST(FsmTest, WaterIntakeTransitionsWhenFull) {
    StubHeater h; StubPump p; StubDoor d;
    // Do NOT pre-set atTarget — keeps controller in HEATING so the poll can detect it
    CycleController ctrl(h, p, d, 1);
    ctrl.start();
    EXPECT_TRUE(waitFor(ctrl, CycleState::WATER_INTAKE, 1000ms));
    p.setLevel(CycleController::FILL_TARGET_L);
    EXPECT_TRUE(waitFor(ctrl, CycleState::HEATING, 1000ms));
    h.setAtTarget(true);  // let cycle finish cleanly on destructor
}

TEST(FsmTest, HeatingTransitionsWhenAtTarget) {
    StubHeater h; StubPump p; StubDoor d;
    p.setLevel(CycleController::FILL_TARGET_L);
    CycleController ctrl(h, p, d);
    ctrl.start();
    EXPECT_TRUE(waitFor(ctrl, CycleState::HEATING, 1000ms));
    h.setAtTarget(true);
    EXPECT_TRUE(waitFor(ctrl, CycleState::PROCESSING, 1000ms));
}

TEST(FsmTest, EmergencyStopTransitionsToError) {
    StubHeater h; StubPump p; StubDoor d;
    CycleController ctrl(h, p, d);
    ctrl.start();
    EXPECT_TRUE(waitFor(ctrl, CycleState::INITIALISING, 500ms));
    ctrl.emergencyStop();
    EXPECT_EQ(ctrl.state(), CycleState::ERROR);
}

TEST(FsmTest, ResetFromErrorReturnsToIdle) {
    StubHeater h; StubPump p; StubDoor d;
    CycleController ctrl(h, p, d);
    ctrl.start();
    waitFor(ctrl, CycleState::INITIALISING, 500ms);
    ctrl.emergencyStop();
    EXPECT_EQ(ctrl.state(), CycleState::ERROR);
    ctrl.reset();
    EXPECT_EQ(ctrl.state(), CycleState::IDLE);
}

TEST(FsmTest, DoorUnlockDuringWaterIntakeCausesError) {
    StubHeater h; StubPump p; StubDoor d;
    h.setAtTarget(true);
    CycleController ctrl(h, p, d);
    ctrl.start();
    EXPECT_TRUE(waitFor(ctrl, CycleState::WATER_INTAKE, 1000ms));
    d.forceUnlock();
    EXPECT_TRUE(waitFor(ctrl, CycleState::ERROR, 1000ms));
    auto st = ctrl.status();
    EXPECT_FALSE(st.errorMsg.empty());
}

TEST(FsmTest, DoorUnlockDuringHeatingCausesError) {
    StubHeater h; StubPump p; StubDoor d;
    p.setLevel(CycleController::FILL_TARGET_L);
    CycleController ctrl(h, p, d);
    ctrl.start();
    EXPECT_TRUE(waitFor(ctrl, CycleState::HEATING, 1000ms));
    d.forceUnlock();
    EXPECT_TRUE(waitFor(ctrl, CycleState::ERROR, 1000ms));
}

TEST(FsmTest, StatusReportsCorrectState) {
    StubHeater h; StubPump p; StubDoor d;
    CycleController ctrl(h, p, d);
    auto st = ctrl.status();
    EXPECT_EQ(st.state, CycleState::ERROR == st.state ? CycleState::ERROR : CycleState::IDLE);
    EXPECT_EQ(st.state, CycleState::IDLE);
}

TEST(FsmTest, DrainCompletesAndReachesComplete) {
    StubHeater h; StubPump p; StubDoor d;
    p.setLevel(CycleController::FILL_TARGET_L);
    h.setAtTarget(true);
    CycleController ctrl(h, p, d, /*processSecs=*/1);  // short processing for test
    ctrl.start();
    EXPECT_TRUE(waitFor(ctrl, CycleState::DRAINING, 5000ms));
    p.setLevel(0.0);
    EXPECT_TRUE(waitFor(ctrl, CycleState::COMPLETE, 2000ms));
}
