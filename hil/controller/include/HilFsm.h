#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

// Five-state HiL manufacturing FSM
enum class HilState { STANDBY, INITIALISING, RUNNING, FAULT, EMERGENCY_STOP };

inline const char* hilStateName(HilState s) {
    switch (s) {
        case HilState::STANDBY:        return "STANDBY";
        case HilState::INITIALISING:   return "INITIALISING";
        case HilState::RUNNING:        return "RUNNING";
        case HilState::FAULT:          return "FAULT";
        case HilState::EMERGENCY_STOP: return "EMERGENCY_STOP";
    }
    return "UNKNOWN";
}

// Thread-safe sensor frame updated by MQTT subscriber thread
struct SensorFrame {
    double   conveyor_pos_m  = 0.0;
    double   spindle_temp_c  = 20.0;
    double   clamp_pressure  = 1.0;
    int64_t  ts_us           = 0;       // source timestamp for latency calc
    bool     valid           = false;
};

class SensorBuffer {
public:
    void update(const SensorFrame& f) {
        std::lock_guard<std::mutex> lk(mu_);
        frame_ = f;
    }
    SensorFrame get() const {
        std::lock_guard<std::mutex> lk(mu_);
        return frame_;
    }
private:
    mutable std::mutex mu_;
    SensorFrame frame_;
};

// PID-style setpoint: simple proportional correction for demo
struct Setpoint {
    double conveyor_speed_ms = 0.5;   // target m/s
    double spindle_coolrate  = 0.0;   // cooling command
};

class HilFsm {
public:
    HilFsm() = default;

    // Called by main loop — drives transitions based on sensor buffer
    HilState step(const SensorBuffer& buf);

    void emergencyStop();
    void reset();          // FAULT or EMERGENCY_STOP → STANDBY

    HilState state() const { return state_.load(); }

    // Compute actuator commands from current sensor reading
    Setpoint computeSetpoint(const SensorFrame& f) const;

    // Transition history for telemetry
    struct Transition { std::string from, to, reason; };
    std::vector<Transition> drainTransitions();

private:
    void transition(HilState next, const std::string& reason = {});

    std::atomic<HilState>         state_{HilState::STANDBY};
    mutable std::mutex            hist_mu_;
    std::vector<Transition>       history_;

    int   initCounter_ = 0;     // counts cycles in INITIALISING
    static constexpr int INIT_CYCLES = 5;
    static constexpr double TEMP_FAULT_C = 85.0;
    static constexpr double PRESS_FAULT  = 5.0;
};
