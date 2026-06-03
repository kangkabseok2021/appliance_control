#pragma once
#include "IBatchComponent.h"
#include <random>
#include <string>

class PressureSensor final : public IBatchComponent {
public:
    explicit PressureSensor(std::string_view id,
                            double nominal_bar = 2.0, double noise_sigma = 0.05,
                            unsigned seed = 99);

    bool init()             override;
    bool selfTest()         override;
    void setpoint(double v) override;  // override nominal pressure (for tests)
    [[nodiscard]] double      read()  const override;  // current pressure bar
    [[nodiscard]] std::string_view name()  const override;

    void injectPressure(double bar) noexcept;

private:
    std::string  id_;
    double       nominal_;
    mutable std::mt19937  rng_;
    mutable std::normal_distribution<double> noise_;
    bool         override_active_{false};
    double       override_val_{0.0};
};
