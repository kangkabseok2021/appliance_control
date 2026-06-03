#include "SimulatedAdsLink.h"
#include <format>
#include <stdexcept>

static void checkAddr(uint16_t addr, int size, const char* op) {
    if (addr >= size)
        throw std::out_of_range(std::format("{}: address {} out of range (max {})", op, addr, size-1));
}

bool SimulatedAdsLink::readCoil(uint16_t addr) const {
    checkAddr(addr, kSize, "readCoil");
    bool v = coils_[addr];
    std::cout << std::format("[ADS] readCoil({})  = {}\n", addr, v);
    return v;
}

void SimulatedAdsLink::writeCoil(uint16_t addr, bool val) {
    checkAddr(addr, kSize, "writeCoil");
    coils_[addr] = val;
    std::cout << std::format("[ADS] writeCoil({}) = {}\n", addr, val);
}

uint16_t SimulatedAdsLink::readRegister(uint16_t addr) const {
    checkAddr(addr, kSize, "readRegister");
    uint16_t v = regs_[addr];
    std::cout << std::format("[ADS] readReg({})   = {}\n", addr, v);
    return v;
}

void SimulatedAdsLink::writeRegister(uint16_t addr, uint16_t val) {
    checkAddr(addr, kSize, "writeRegister");
    regs_[addr] = val;
    std::cout << std::format("[ADS] writeReg({})  = {}\n", addr, val);
}
