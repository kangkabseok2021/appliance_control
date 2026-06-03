#include "Valve.h"
#include "BatchComponent.h"

Valve::Valve(std::string_view id, bool inject_leak)
    : id_(id), leak_(inject_leak) {}

bool Valve::init()             { open_ = false; return true; }
bool Valve::selfTest()         { return !leak_; }
void Valve::setpoint(double v) { open_ = (v >= 0.5); }
double Valve::read() const     { return open_ ? 1.0 : 0.0; }
std::string_view Valve::name() const { return id_; }
bool Valve::isOpen()  const noexcept { return open_; }
bool Valve::hasLeak() const noexcept { return leak_; }

static_assert(BatchComponent<Valve>, "Valve must satisfy BatchComponent");
