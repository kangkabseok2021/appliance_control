#pragma once
#include "IBatchComponent.h"
#include <random>
#include <string>

class WeighingScale final : public IBatchComponent {
public:
    // flow_rate_kgps: kg per second when valve is open
    explicit WeighingScale(std::string_view id, double flow_rate_kgps = 2.0,
                           unsigned seed = 42);

    bool init()             override;
    bool selfTest()         override;
    void setpoint(double v) override;  // 0 = stop filling, 1 = filling
    [[nodiscard]] double      read()  const override; // accumulated mass kg
    [[nodiscard]] std::string_view name()  const override;

    // Advance simulation by dt seconds
    void update(double dt);
    void reset();

private:
    std::string      id_;
    double           flow_rate_;
    double           mass_{0.0};
    bool             filling_{false};
    std::mt19937     rng_;
    std::normal_distribution<double> noise_{0.0, 0.02};
};
