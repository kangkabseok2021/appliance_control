#pragma once

class IDoorLock {
public:
    virtual ~IDoorLock() = default;
    virtual bool lock() = 0;
    virtual bool unlock() = 0;
    virtual bool isLocked() const = 0;
};
