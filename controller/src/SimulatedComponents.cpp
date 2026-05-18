#include "SimulatedComponents.h"
#include <chrono>

using namespace std::chrono_literals;

bool SimulatedHeaterControl::isAtTarget() const {
    double t = readTemp();
    double tgt = target_.load();
    bool within = (t >= tgt - 0.5 && t <= tgt + 0.5);
    auto now = std::chrono::steady_clock::now();
    if (within) {
        if (!inRange_) {
            inRange_ = true;
            inRangeSince_ = now;
        }
        return std::chrono::duration_cast<std::chrono::seconds>(
            now - inRangeSince_
        ).count() >= 5;  // 5s stable (shortened from 30s for demo)
    }
    inRange_ = false;
    return false;
}
