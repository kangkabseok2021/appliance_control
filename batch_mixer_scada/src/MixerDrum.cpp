#include "MixerDrum.h"
#include "BatchComponent.h"

MixerDrum::MixerDrum(std::string_view id, double k_heat, double k_cool, double t_ambient)
    : id_(id), temp_(t_ambient), t_ambient_(t_ambient), k_heat_(k_heat), k_cool_(k_cool) {}

bool MixerDrum::init()             { rpm_ = 0.0; target_rpm_ = 0.0; temp_ = t_ambient_; return true; }
bool MixerDrum::selfTest()         { return true; }
void MixerDrum::setpoint(double v) { target_rpm_ = v; }
double MixerDrum::read() const     { return rpm_; }
double MixerDrum::temperature() const noexcept { return temp_; }
std::string_view MixerDrum::name() const { return id_; }

void MixerDrum::update(double dt) {
    // RPM ramp: instant set for simulation simplicity
    rpm_ = target_rpm_;
    // Temperature ODE: dT/dt = k_heat * RPM - k_cool * (T - T_ambient)
    double dT = k_heat_ * rpm_ - k_cool_ * (temp_ - t_ambient_);
    temp_ += dT * dt;
}

void MixerDrum::stop() { target_rpm_ = 0.0; rpm_ = 0.0; }

static_assert(BatchComponent<MixerDrum>, "MixerDrum must satisfy BatchComponent");
