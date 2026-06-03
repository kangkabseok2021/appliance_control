#pragma once
#include "IBatchComponent.h"
#include "Valve.h"
#include "WeighingScale.h"
#include "MixerDrum.h"
#include "PressureSensor.h"
#include "PressureGuard.h"
#include "ITwinCatLink.h"
#include <string>
#include <cstdint>

enum class BatchState {
    IDLE,
    SILO_DRAIN,
    SCALE_FILL,
    MIXING,
    DISCHARGE,
    EMERGENCY_STOP
};

struct RecipeConfig {
    int         id{0};
    std::string name;
    double      material_a_kg{0.0};
    double      material_b_kg{0.0};
    int         mix_duration_s{0};
    int         target_rpm{0};
    double      max_pressure_bar{6.0};
};

class BatchController {
public:
    // Owns all HAL components by reference; caller must ensure lifetime.
    BatchController(Valve& silo_a, Valve& silo_b, Valve& discharge,
                    WeighingScale& scale_a, WeighingScale& scale_b,
                    MixerDrum& drum, PressureSensor& pressure,
                    ITwinCatLink& ads);

    void loadRecipe(const RecipeConfig& recipe);

    // Returns false if selfTest fails or already running
    bool start();

    // Advance FSM by dt seconds; returns current state
    BatchState step(double dt);

    void acknowledge();  // E_STOP → IDLE if guard clear

    [[nodiscard]] BatchState    state()    const noexcept;
    [[nodiscard]] const RecipeConfig& recipe() const noexcept;

private:
    static constexpr double kMassTolerance = 0.5;  // kg

    Valve&          silo_a_;
    Valve&          silo_b_;
    Valve&          discharge_;
    WeighingScale&  scale_a_;
    WeighingScale&  scale_b_;
    MixerDrum&      drum_;
    PressureSensor& pressure_;
    ITwinCatLink&   ads_;
    RecipeConfig    recipe_;     // declared before guard_: C++ inits in decl order
    PressureGuard   guard_;

    BatchState      state_{BatchState::IDLE};
    double          elapsed_s_{0.0};
};
