#pragma once
#include <atomic>
#include <thread>
#include "PressureSensor.h"

// Cooperative safety watchdog using std::jthread + std::stop_token.
// Polls PressureSensor every 100 ms; trips when pressure > threshold.
// The jthread auto-joins on PressureGuard destruction — no explicit join needed.
class PressureGuard {
public:
    PressureGuard(PressureSensor& sensor, double threshold_bar);
    ~PressureGuard() = default;  // jthread RAII handles shutdown

    // Non-copyable, non-movable (jthread holds a reference to sensor)
    PressureGuard(const PressureGuard&)            = delete;
    PressureGuard& operator=(const PressureGuard&) = delete;

    [[nodiscard]] bool isTripped() const noexcept;

    // Reset latch — only succeeds when current pressure < threshold.
    bool acknowledge() noexcept;

private:
    PressureSensor&       sensor_;
    double                threshold_;
    std::atomic<bool>     tripped_{false};
    std::jthread          watcher_;
};
