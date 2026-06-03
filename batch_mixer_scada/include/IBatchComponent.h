#pragma once
#include <string_view>

// Virtual base for runtime-polymorphic HAL dispatch inside BatchController.
// All concrete components also satisfy the BatchComponent concept (static_asserted
// at the bottom of each .cpp).
class IBatchComponent {
public:
    virtual ~IBatchComponent() = default;
    virtual bool        init()             = 0;
    virtual bool        selfTest()         = 0;
    virtual void        setpoint(double v) = 0;
    [[nodiscard]] virtual double      read() const = 0;
    [[nodiscard]] virtual std::string_view name() const = 0;
};
