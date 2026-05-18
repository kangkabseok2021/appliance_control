#pragma once
#include "IHeaterControl.h"
#include "IPumpControl.h"
#include "IDoorLock.h"
#include <atomic>
#include <chrono>
#include <mutex>

// Simulated HAL components — updated by the socket reader thread.
// In production these would wrap real hardware drivers.

struct SensorFrame {
    double tempC       = 20.0;
    double waterLevelL = 0.0;
    bool   doorLocked  = false;
    bool   valid       = false;
};

class SharedSensorState {
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

class SimulatedHeaterControl : public IHeaterControl {
public:
    explicit SimulatedHeaterControl(SharedSensorState& s) : sensor_(s) {}
    void   setTarget(double c) override { target_ = c; }
    double readTemp()   const override  { return sensor_.get().tempC; }
    bool   isAtTarget() const override;
private:
    SharedSensorState& sensor_;
    std::atomic<double> target_{0.0};
    // Track how long we've been within tolerance
    mutable std::chrono::steady_clock::time_point inRangeSince_{};
    mutable bool inRange_ = false;
};

class SimulatedPumpControl : public IPumpControl {
public:
    explicit SimulatedPumpControl(SharedSensorState& s) : sensor_(s) {}
    void   startFill()  override { mode_ = 1; }
    void   startDrain() override { mode_ = -1; }
    void   stop()       override { mode_ = 0; }
    double readLevel()  const override { return sensor_.get().waterLevelL; }
    int    mode() const { return mode_; }
private:
    SharedSensorState& sensor_;
    std::atomic<int>   mode_{0};  // 1=fill, -1=drain, 0=off
};

class SimulatedDoorLock : public IDoorLock {
public:
    explicit SimulatedDoorLock(SharedSensorState& s) : sensor_(s) {}
    bool lock()     override { locked_ = true;  return true; }
    bool unlock()   override { locked_ = false; return true; }
    bool isLocked() const override {
        // Physical door state overrides software lock (safety)
        return locked_ && sensor_.get().doorLocked;
    }
private:
    SharedSensorState& sensor_;
    std::atomic<bool>  locked_{false};
};
