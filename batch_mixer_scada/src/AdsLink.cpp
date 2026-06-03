#include "AdsLink.h"
#include <stdexcept>
#include <format>

// Stub implementation. Real calls would use TcAdsDll / TcAdsLib:
//   AdsOpenPort() → get port handle
//   AdsSyncReadReqEx2(port, &addr, ADSIGRP_SYM_VAL, offset, sizeof(T), &val)
//   AdsSyncWriteReqEx(port, &addr, ADSIGRP_SYM_VAL, offset, sizeof(T), &val)

AdsLink::AdsLink(const char* host, uint16_t port) : host_(host), port_(port) {}

bool AdsLink::readCoil(uint16_t addr) const {
    throw std::runtime_error(std::format(
        "AdsLink not connected ({}:{}). Deploy with TwinCAT runtime.", host_, port_));
}

void AdsLink::writeCoil(uint16_t addr, bool val) {
    throw std::runtime_error(std::format(
        "AdsLink not connected ({}:{}). Deploy with TwinCAT runtime.", host_, port_));
}

uint16_t AdsLink::readRegister(uint16_t addr) const {
    throw std::runtime_error(std::format(
        "AdsLink not connected ({}:{}). Deploy with TwinCAT runtime.", host_, port_));
}

void AdsLink::writeRegister(uint16_t addr, uint16_t val) {
    throw std::runtime_error(std::format(
        "AdsLink not connected ({}:{}). Deploy with TwinCAT runtime.", host_, port_));
}
