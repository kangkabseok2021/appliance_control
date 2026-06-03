#include "WeighingScale.h"
#include "BatchComponent.h"
#include <cmath>

WeighingScale::WeighingScale(std::string_view id, double flow_rate_kgps, unsigned seed)
    : id_(id), flow_rate_(flow_rate_kgps), rng_(seed) {}

bool WeighingScale::init()             { mass_ = 0.0; filling_ = false; return true; }
bool WeighingScale::selfTest()         { return true; }
void WeighingScale::setpoint(double v) { filling_ = (v >= 0.5); }
double WeighingScale::read() const     { return mass_; }
std::string_view WeighingScale::name() const { return id_; }

void WeighingScale::update(double dt) {
    if (filling_)
        mass_ += flow_rate_ * dt + noise_(rng_);
}

void WeighingScale::reset() { mass_ = 0.0; filling_ = false; }

static_assert(BatchComponent<WeighingScale>, "WeighingScale must satisfy BatchComponent");
