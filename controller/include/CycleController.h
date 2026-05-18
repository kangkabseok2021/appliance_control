#pragma once
#include "CycleState.h"
#include "IHeaterControl.h"
#include "IPumpControl.h"
#include "IDoorLock.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

struct CycleStatus {
    CycleState state;
    double     tempC;
    double     waterLevelL;
    bool       doorLocked;
    int        elapsedSec;
    std::string errorMsg;
};

class CycleController {
public:
    static constexpr double FILL_TARGET_L    = 8.0;
    static constexpr double HEAT_TARGET_C    = 85.0;
    static constexpr int    PROCESS_SECS     = 30;   // default; override via ctor for tests
    static constexpr double DRAIN_DONE_L     = 0.3;

    CycleController(IHeaterControl& heater, IPumpControl& pump, IDoorLock& door,
                    int processSecs = PROCESS_SECS);
    ~CycleController();

    void start();            // IDLE → INITIALISING
    void emergencyStop();    // any → ERROR
    void reset();            // COMPLETE or ERROR → IDLE

    CycleStatus status() const;
    CycleState  state()  const { return state_; }

private:
    void run();
    void transition(CycleState next, const std::string& error = {});

    IHeaterControl& heater_;
    IPumpControl&   pump_;
    IDoorLock&      door_;

    std::atomic<CycleState> state_{CycleState::IDLE};
    mutable std::mutex      statusMu_;
    std::string             errorMsg_;

    int               processSecs_;
    std::atomic<bool> running_{false};
    std::thread       worker_;

    std::chrono::steady_clock::time_point cycleStart_;
    std::chrono::steady_clock::time_point processingStart_;
};
