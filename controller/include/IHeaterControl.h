#pragma once

class IHeaterControl {
public:
    virtual ~IHeaterControl() = default;
    virtual void setTarget(double celsius) = 0;
    virtual double readTemp() const = 0;
    virtual bool isAtTarget() const = 0;  // within ±0.5°C for 30s
};
