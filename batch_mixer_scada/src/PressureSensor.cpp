#include "PressureSensor.h"
#include "BatchComponent.h"

PressureSensor::PressureSensor(std::string_view id, double nominal_bar,
                                double noise_sigma, unsigned seed)
    : id_(id), nominal_(nominal_bar), rng_(seed), noise_(0.0, noise_sigma) {}

bool PressureSensor::init()             { override_active_ = false; return true; }
bool PressureSensor::selfTest()         { return true; }
void PressureSensor::setpoint(double v) { injectPressure(v); }

double PressureSensor::read() const {
    if (override_active_) return override_val_;
    return nominal_ + noise_(rng_);
}

std::string_view PressureSensor::name() const { return id_; }

void PressureSensor::injectPressure(double bar) noexcept {
    override_val_    = bar;
    override_active_ = true;
}

static_assert(BatchComponent<PressureSensor>, "PressureSensor must satisfy BatchComponent");
