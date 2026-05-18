#include "CycleController.h"
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;

CycleController::CycleController(IHeaterControl& heater, IPumpControl& pump, IDoorLock& door,
                                 int processSecs)
    : heater_(heater), pump_(pump), door_(door), processSecs_(processSecs) {}

CycleController::~CycleController() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

void CycleController::start() {
    if (state_.load() != CycleState::IDLE)
        throw std::logic_error("start() called outside IDLE state");
    // Use exchange to atomically guard against double-start race
    if (running_.exchange(true))
        throw std::logic_error("Cycle already running");
    cycleStart_ = std::chrono::steady_clock::now();
    worker_ = std::thread([this]{ run(); });
}

void CycleController::emergencyStop() {
    transition(CycleState::ERROR, "Emergency stop triggered");
    running_ = false;
}

void CycleController::reset() {
    CycleState cur = state_.load();
    if (cur != CycleState::COMPLETE && cur != CycleState::ERROR)
        throw std::logic_error("reset() only valid from COMPLETE or ERROR");
    pump_.stop();
    {
        std::lock_guard lk(statusMu_);
        errorMsg_.clear();
    }
    door_.unlock();
    state_ = CycleState::IDLE;
}

CycleStatus CycleController::status() const {
    auto now = std::chrono::steady_clock::now();
    int elapsed = static_cast<int>(
        std::chrono::duration_cast<std::chrono::seconds>(now - cycleStart_).count()
    );
    std::lock_guard lk(statusMu_);
    return {
        state_.load(),
        heater_.readTemp(),
        pump_.readLevel(),
        door_.isLocked(),
        elapsed,
        errorMsg_
    };
}

void CycleController::transition(CycleState next, const std::string& error) {
    std::lock_guard lk(statusMu_);
    errorMsg_ = error;
    state_ = next;
}

void CycleController::run() {
    // INITIALISING — lock door, verify sensors
    transition(CycleState::INITIALISING);
    if (!door_.lock()) {
        transition(CycleState::ERROR, "Door lock failed during INITIALISING");
        return;
    }
    std::this_thread::sleep_for(500ms);  // self-test delay

    // WATER_INTAKE — fill to target
    transition(CycleState::WATER_INTAKE);
    pump_.startFill();
    while (running_ && state_ == CycleState::WATER_INTAKE) {
        if (!door_.isLocked()) {
            pump_.stop();
            transition(CycleState::ERROR, "Door unlocked during WATER_INTAKE");
            return;
        }
        if (pump_.readLevel() >= FILL_TARGET_L) break;
        std::this_thread::sleep_for(100ms);
    }
    pump_.stop();
    if (!running_) return;

    // HEATING — bring to setpoint
    transition(CycleState::HEATING);
    heater_.setTarget(HEAT_TARGET_C);
    while (running_ && state_ == CycleState::HEATING) {
        if (!door_.isLocked()) {
            heater_.setTarget(0);
            pump_.stop();
            transition(CycleState::ERROR, "Door unlocked during HEATING");
            return;
        }
        if (heater_.isAtTarget()) break;
        std::this_thread::sleep_for(100ms);
    }
    if (!running_) return;

    // PROCESSING — hold temperature for cycle duration
    transition(CycleState::PROCESSING);
    processingStart_ = std::chrono::steady_clock::now();
    while (running_ && state_ == CycleState::PROCESSING) {
        if (!door_.isLocked()) {
            heater_.setTarget(0);
            pump_.stop();
            transition(CycleState::ERROR, "Door unlocked during PROCESSING");
            return;
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - processingStart_
        ).count();
        if (elapsed >= processSecs_) break;
        std::this_thread::sleep_for(100ms);
    }
    heater_.setTarget(0);
    if (!running_) return;

    // DRAINING — empty tank
    transition(CycleState::DRAINING);
    pump_.startDrain();
    while (running_ && state_ == CycleState::DRAINING) {
        if (pump_.readLevel() <= DRAIN_DONE_L) break;
        std::this_thread::sleep_for(100ms);
    }
    pump_.stop();
    if (!running_) return;

    door_.unlock();
    transition(CycleState::COMPLETE);
    running_ = false;
}
