#pragma once
#include "IBatchComponent.h"
#include <string>

class MixerDrum final : public IBatchComponent {
public:
    explicit MixerDrum(std::string_view id,
                       double k_heat = 0.05, double k_cool = 0.02,
                       double t_ambient = 20.0);

    bool init()             override;
    bool selfTest()         override;
    void setpoint(double v) override;   // target RPM
    [[nodiscard]] double      read()  const override;  // current RPM
    [[nodiscard]] std::string_view name()  const override;

    [[nodiscard]] double temperature() const noexcept;
    void update(double dt);
    void stop();

private:
    std::string id_;
    double target_rpm_{0.0};
    double rpm_{0.0};
    double temp_;
    double t_ambient_;
    double k_heat_, k_cool_;
};
