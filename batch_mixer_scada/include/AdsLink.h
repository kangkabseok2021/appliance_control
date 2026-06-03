#pragma once
#include "ITwinCatLink.h"

// Stub for real Beckhoff TcAdsLib integration.
// Replace SimulatedAdsLink with this class when running against a real
// TwinCAT runtime on ADS port 801.
//
// Real usage (requires TcAdsDll.h from Beckhoff Automation SDK):
//   AmsAddr addr = { {192,168,1,1,1,1}, 801 };   // AMS net-id + port
//   AdsOpenPort();
//   AdsSyncReadReqEx2(port, &addr, ADSIGRP_SYM_VAL, offset, ...);
class AdsLink final : public ITwinCatLink {
public:
    // host: TwinCAT runtime IP; port: ADS port (801 = TwinCAT 3 PLC runtime)
    explicit AdsLink(const char* host, uint16_t port = 801);

    [[nodiscard]] bool     readCoil(uint16_t addr)          const override;
    void   writeCoil(uint16_t addr, bool val)                     override;
    [[nodiscard]] uint16_t readRegister(uint16_t addr)      const override;
    void   writeRegister(uint16_t addr, uint16_t val)             override;

private:
    const char* host_;
    uint16_t    port_;
};
