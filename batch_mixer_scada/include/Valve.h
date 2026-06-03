#pragma once
#include "IBatchComponent.h"
#include <string>

class Valve final : public IBatchComponent {
public:
    explicit Valve(std::string_view id, bool inject_leak = false);

    bool init()             override;
    bool selfTest()         override;
    void setpoint(double v) override;   // 0.0 = close, 1.0 = open
    [[nodiscard]] double      read()  const override; // 0.0 or 1.0
    [[nodiscard]] std::string_view name()  const override;

    [[nodiscard]] bool isOpen()   const noexcept;
    [[nodiscard]] bool hasLeak()  const noexcept;

private:
    std::string id_;
    bool open_{false};
    bool leak_;
};
