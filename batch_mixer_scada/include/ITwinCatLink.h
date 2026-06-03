#pragma once
#include <cstdint>

// Abstract interface modelling a Beckhoff TwinCAT ADS communications channel.
// SimulatedAdsLink: in-process memory map (unit-testable, no real PLC).
// AdsLink: stub with real TcAdsLib call sites (requires TwinCAT runtime port 801).
class ITwinCatLink {
public:
    virtual ~ITwinCatLink() = default;
    [[nodiscard]] virtual bool     readCoil(uint16_t addr)              const = 0;
    virtual void   writeCoil(uint16_t addr, bool val)                         = 0;
    [[nodiscard]] virtual uint16_t readRegister(uint16_t addr)          const = 0;
    virtual void   writeRegister(uint16_t addr, uint16_t val)                 = 0;
};
