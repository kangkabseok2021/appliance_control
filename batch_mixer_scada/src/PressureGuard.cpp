#include "PressureGuard.h"
#include <chrono>
#include <format>
#include <iostream>

using namespace std::chrono_literals;

PressureGuard::PressureGuard(PressureSensor& sensor, double threshold_bar)
    : sensor_(sensor), threshold_(threshold_bar)
{
    watcher_ = std::jthread([this](std::stop_token stoken) {
        while (!stoken.stop_requested()) {
            double p = sensor_.read();
            if (p > threshold_) {
                tripped_.store(true, std::memory_order_release);
                std::cout << std::format(
                    "[PressureGuard] TRIP: {:.2f} bar > {:.2f} bar threshold\n",
                    p, threshold_);
            }
            std::this_thread::sleep_for(100ms);
        }
    });
}

bool PressureGuard::isTripped() const noexcept {
    return tripped_.load(std::memory_order_acquire);
}

bool PressureGuard::acknowledge() noexcept {
    double p = sensor_.read();
    if (p >= threshold_) return false;  // still over-pressure — deny
    tripped_.store(false, std::memory_order_release);
    return true;
}
