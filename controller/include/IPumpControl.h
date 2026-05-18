#pragma once

class IPumpControl {
public:
    virtual ~IPumpControl() = default;
    virtual void startFill() = 0;
    virtual void startDrain() = 0;
    virtual void stop() = 0;
    virtual double readLevel() const = 0;  // litres
};
