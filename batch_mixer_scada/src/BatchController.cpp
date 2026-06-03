#include "BatchController.h"
#include <format>
#include <iostream>
#include <stdexcept>

BatchController::BatchController(
    Valve& silo_a, Valve& silo_b, Valve& discharge,
    WeighingScale& scale_a, WeighingScale& scale_b,
    MixerDrum& drum, PressureSensor& pressure, ITwinCatLink& ads)
    : silo_a_(silo_a), silo_b_(silo_b), discharge_(discharge),
      scale_a_(scale_a), scale_b_(scale_b),
      drum_(drum), pressure_(pressure), ads_(ads),
      guard_(pressure_, recipe_.max_pressure_bar)
{}

void BatchController::loadRecipe(const RecipeConfig& recipe) {
    if (state_ != BatchState::IDLE)
        throw std::logic_error("Cannot load recipe while batch is running");
    recipe_ = recipe;
}

bool BatchController::start() {
    if (state_ != BatchState::IDLE) return false;
    // selfTest all HAL components before allowing start
    if (!silo_a_.selfTest() || !silo_b_.selfTest() || !discharge_.selfTest() ||
        !scale_a_.selfTest() || !scale_b_.selfTest() ||
        !drum_.selfTest() || !pressure_.selfTest())
        return false;

    scale_a_.reset();
    scale_b_.reset();
    elapsed_s_ = 0.0;
    state_ = BatchState::SILO_DRAIN;
    silo_a_.setpoint(1.0);
    scale_a_.setpoint(1.0);
    std::cout << std::format("[Batch] Started recipe '{}' run\n", recipe_.name);
    return true;
}

BatchState BatchController::step(double dt) {
    // Emergency stop takes priority from any state
    if (guard_.isTripped() && state_ != BatchState::EMERGENCY_STOP) {
        silo_a_.setpoint(0.0);
        silo_b_.setpoint(0.0);
        discharge_.setpoint(0.0);
        drum_.stop();
        state_ = BatchState::EMERGENCY_STOP;
        std::cout << std::format("[Batch] EMERGENCY_STOP (pressure guard tripped)\n");
        return state_;
    }

    switch (state_) {
    case BatchState::SILO_DRAIN:
        scale_a_.update(dt);
        if (scale_a_.read() >= recipe_.material_a_kg - kMassTolerance) {
            silo_a_.setpoint(0.0);
            scale_a_.setpoint(0.0);
            silo_b_.setpoint(1.0);
            scale_b_.setpoint(1.0);
            state_ = BatchState::SCALE_FILL;
        }
        break;

    case BatchState::SCALE_FILL:
        scale_b_.update(dt);
        if (scale_b_.read() >= recipe_.material_b_kg - kMassTolerance) {
            silo_b_.setpoint(0.0);
            scale_b_.setpoint(0.0);
            drum_.setpoint(static_cast<double>(recipe_.target_rpm));
            state_ = BatchState::MIXING;
        }
        break;

    case BatchState::MIXING:
        drum_.update(dt);
        elapsed_s_ += dt;
        if (elapsed_s_ >= static_cast<double>(recipe_.mix_duration_s)) {
            drum_.stop();
            discharge_.setpoint(1.0);
            state_ = BatchState::DISCHARGE;
        }
        break;

    case BatchState::DISCHARGE:
        // One step to simulate discharge; then complete
        discharge_.setpoint(0.0);
        state_ = BatchState::IDLE;
        std::cout << std::format("[Batch] Batch complete — recipe '{}'\n", recipe_.name);
        break;

    case BatchState::IDLE:
    case BatchState::EMERGENCY_STOP:
        break;
    }
    return state_;
}

void BatchController::acknowledge() {
    if (state_ != BatchState::EMERGENCY_STOP) return;
    if (guard_.acknowledge()) {
        state_ = BatchState::IDLE;
        std::cout << std::format("[Batch] E-STOP acknowledged — returning to IDLE\n");
    }
}

BatchState BatchController::state() const noexcept { return state_; }
const RecipeConfig& BatchController::recipe() const noexcept { return recipe_; }
