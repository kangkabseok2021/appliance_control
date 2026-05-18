#pragma once
#include "SimulatedComponents.h"
#include "../third_party/json.hpp"
#include <atomic>
#include <string>
#include <thread>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// Connects to the Python simulator Unix socket and keeps SharedSensorState updated.
class SocketSensorReader {
public:
    SocketSensorReader(SharedSensorState& state, SimulatedPumpControl& pump,
                       const std::string& socketPath)
        : state_(state), pump_(pump), path_(socketPath) {}

    ~SocketSensorReader() {
        stop();
    }

    void start() {
        running_ = true;
        thread_ = std::thread([this]{ run(); });
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
    }

private:
    void run() {
        while (running_) {
            int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd < 0) { std::this_thread::sleep_for(std::chrono::seconds(1)); continue; }

            sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            ::strncpy(addr.sun_path, path_.c_str(), sizeof(addr.sun_path) - 1);

            if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
                ::close(fd);
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            char buf[4096];
            std::string partial;
            while (running_) {
                ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
                if (n <= 0) break;
                buf[n] = '\0';
                partial += buf;

                size_t pos;
                while ((pos = partial.find('\n')) != std::string::npos) {
                    std::string line = partial.substr(0, pos);
                    partial = partial.substr(pos + 1);
                    try {
                        auto j = nlohmann::json::parse(line);
                        SensorFrame f;
                        f.tempC       = j.value("temp_c", 20.0);
                        f.waterLevelL = j.value("water_level_l", 0.0);
                        f.doorLocked  = j.value("door_locked", false);
                        f.valid       = true;
                        state_.update(f);

                        // Send pump command back to simulator
                        nlohmann::json cmd;
                        cmd["cmd"]  = "PUMP_MODE";
                        cmd["mode"] = pump_.mode();
                        std::string out = cmd.dump() + "\n";
                        ::write(fd, out.c_str(), out.size());
                    } catch (...) {}
                }
            }
            ::close(fd);
        }
    }

    SharedSensorState&     state_;
    SimulatedPumpControl&  pump_;
    std::string            path_;
    std::atomic<bool>      running_{false};
    std::thread            thread_;
};
