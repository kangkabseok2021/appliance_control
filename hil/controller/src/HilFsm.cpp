#include "HilFsm.h"

void HilFsm::transition(HilState next, const std::string& reason) {
    std::string from = hilStateName(state_.load());
    state_           = next;
    std::lock_guard<std::mutex> lk(hist_mu_);
    history_.push_back({from, hilStateName(next), reason});
}

HilState HilFsm::step(const SensorBuffer& buf) {
    auto f = buf.get();
    auto s = state_.load();

    // Safety guard — any state
    if (f.valid) {
        if (f.spindle_temp_c > TEMP_FAULT_C || f.clamp_pressure > PRESS_FAULT) {
            if (s != HilState::FAULT && s != HilState::EMERGENCY_STOP)
                transition(HilState::FAULT, "sensor threshold breach");
            return state_.load();
        }
    }

    switch (s) {
        case HilState::STANDBY:
            transition(HilState::INITIALISING, "start");
            initCounter_ = 0;
            break;

        case HilState::INITIALISING:
            if (!f.valid) break;                // wait for first valid frame
            if (++initCounter_ >= INIT_CYCLES)
                transition(HilState::RUNNING, "init complete");
            break;

        case HilState::RUNNING:
            // Stay running — setpoint published externally every 20ms
            break;

        case HilState::FAULT:
        case HilState::EMERGENCY_STOP:
            break;
    }
    return state_.load();
}

void HilFsm::emergencyStop() {
    transition(HilState::EMERGENCY_STOP, "operator command");
}

void HilFsm::reset() {
    auto s = state_.load();
    if (s == HilState::FAULT || s == HilState::EMERGENCY_STOP)
        transition(HilState::STANDBY, "reset");
}

Setpoint HilFsm::computeSetpoint(const SensorFrame& f) const {
    Setpoint sp;
    // Proportional speed control: slow down if temperature high
    double temp_ratio = f.spindle_temp_c / 80.0;
    sp.conveyor_speed_ms = std::max(0.1, 0.5 * (1.0 - 0.5 * temp_ratio));
    // Request cooling if temp > 60°C
    sp.spindle_coolrate = f.spindle_temp_c > 60.0 ? 1.0 : 0.0;
    return sp;
}

std::vector<HilFsm::Transition> HilFsm::drainTransitions() {
    std::lock_guard<std::mutex> lk(hist_mu_);
    return std::move(history_);
}
