#pragma once
#include "ITwinCatLink.h"
#include <array>
#include <iostream>

// In-process ADS simulation: 64 coils + 64 holding registers.
// Every access is printed to stdout so the PLC dialogue is visible during tests.
class SimulatedAdsLink final : public ITwinCatLink {
public:
    SimulatedAdsLink() = default;

    [[nodiscard]] bool     readCoil(uint16_t addr)          const override;
    void   writeCoil(uint16_t addr, bool val)                     override;
    [[nodiscard]] uint16_t readRegister(uint16_t addr)      const override;
    void   writeRegister(uint16_t addr, uint16_t val)             override;

private:
    static constexpr int kSize = 64;
    std::array<bool,     kSize> coils_{};
    std::array<uint16_t, kSize> regs_{};
};
