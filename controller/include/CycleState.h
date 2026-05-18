#pragma once
#include <string>

enum class CycleState {
    IDLE,
    INITIALISING,
    WATER_INTAKE,
    HEATING,
    PROCESSING,
    DRAINING,
    COMPLETE,
    ERROR
};

inline std::string stateToString(CycleState s) {
    switch (s) {
        case CycleState::IDLE:         return "IDLE";
        case CycleState::INITIALISING: return "INITIALISING";
        case CycleState::WATER_INTAKE: return "WATER_INTAKE";
        case CycleState::HEATING:      return "HEATING";
        case CycleState::PROCESSING:   return "PROCESSING";
        case CycleState::DRAINING:     return "DRAINING";
        case CycleState::COMPLETE:     return "COMPLETE";
        case CycleState::ERROR:        return "ERROR";
    }
    return "UNKNOWN";
}
