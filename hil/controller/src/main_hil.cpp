#include "HilFsm.h"
#include "MqttClient.h"
#include "../third_party/json.hpp"
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;
using json = nlohmann::json;

static std::atomic<bool> g_running{true};
void sigHandler(int) { g_running = false; }

int main(int argc, char* argv[]) {
    std::signal(SIGINT, sigHandler);
    std::signal(SIGTERM, sigHandler);

    std::string broker_host = argc > 1 ? argv[1] : "localhost";
    uint16_t    broker_port = argc > 2 ? static_cast<uint16_t>(std::stoi(argv[2])) : 1883;

    SensorBuffer buf;
    HilFsm       fsm;
    MqttClient   mqtt(broker_host, broker_port, "hil-controller");

    // Subscribe to all sensor topics
    mqtt.subscribe("sensor/#", [&](const std::string& topic, const std::string& payload) {
        try {
            auto j = json::parse(payload);
            SensorFrame f;
            f.conveyor_pos_m = j.value("conveyor_pos_m", 0.0);
            f.spindle_temp_c = j.value("spindle_temp_c", 20.0);
            f.clamp_pressure = j.value("clamp_pressure",  1.0);
            f.ts_us          = j.value("ts_us", int64_t{0});
            f.valid          = true;
            buf.update(f);
        } catch (...) { /* corrupted payload — ignored */ }
    });

    // Connect with retry
    for (int attempt = 0; !mqtt.connect() && attempt < 10; ++attempt) {
        std::cerr << "[hil] broker not ready, retry " << attempt + 1 << "/10\n";
        std::this_thread::sleep_for(std::chrono::seconds(1 << std::min(attempt, 4)));
    }

    if (!mqtt.isConnected()) {
        std::cerr << "[hil] could not connect to broker\n";
        return 1;
    }

    std::cout << "[hil] connected to " << broker_host << ":" << broker_port << "\n";

    // 20 ms control loop — O(1) per iteration
    auto next = std::chrono::steady_clock::now();
    while (g_running) {
        auto state = fsm.step(buf);
        auto frame = buf.get();

        if (state == HilState::RUNNING && frame.valid) {
            auto sp = fsm.computeSetpoint(frame);
            json cmd;
            cmd["conveyor_speed_ms"] = sp.conveyor_speed_ms;
            cmd["spindle_coolrate"]  = sp.spindle_coolrate;
            cmd["controller_ts_us"]  = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            mqtt.publish("command/actuator/conveyor", cmd.dump(), 1);
        }

        // Publish heartbeat state
        json status;
        status["state"] = hilStateName(state);
        mqtt.publish("hil/status", status.dump(), 0);

        next += 20ms;
        std::this_thread::sleep_until(next);
    }

    mqtt.disconnect();
    return 0;
}
