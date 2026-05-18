#include "CycleController.h"
#include "SimulatedComponents.h"
#include "SocketSensorReader.h"
#include "../third_party/httplib.h"
#include "../third_party/json.hpp"
#include <csignal>
#include <iostream>

static std::atomic<bool> g_quit{false};
void sigHandler(int) { g_quit = true; }

int main(int argc, char* argv[]) {
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    std::string sockPath = argc > 1 ? argv[1] : "/tmp/appliance-sim.sock";

    // Dependency injection — swap these for real hardware drivers in production
    SharedSensorState       sensorState;
    SimulatedHeaterControl  heater(sensorState);
    SimulatedPumpControl    pump(sensorState);
    SimulatedDoorLock       door(sensorState);

    SocketSensorReader reader(sensorState, pump, sockPath);
    reader.start();

    CycleController controller(heater, pump, door);

    // HTTP server on :8080
    httplib::Server svr;

    svr.Get("/state", [&](const httplib::Request&, httplib::Response& res) {
        auto st = controller.status();
        nlohmann::json j;
        j["state"]       = stateToString(st.state);
        j["temp_c"]      = st.tempC;
        j["water_l"]     = st.waterLevelL;
        j["door_locked"] = st.doorLocked;
        j["elapsed_sec"] = st.elapsedSec;
        j["error"]       = st.errorMsg;
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/cmd/start", [&](const httplib::Request&, httplib::Response& res) {
        try {
            controller.start();
            res.set_content("{\"ok\":true}", "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(nlohmann::json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Post("/cmd/stop", [&](const httplib::Request&, httplib::Response& res) {
        controller.emergencyStop();
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Post("/cmd/reset", [&](const httplib::Request&, httplib::Response& res) {
        try {
            controller.reset();
            res.set_content("{\"ok\":true}", "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(nlohmann::json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    std::cout << "Appliance controller running on :8080\n";
    std::thread httpThread([&]{ svr.listen("0.0.0.0", 8080); });

    while (!g_quit) std::this_thread::sleep_for(std::chrono::milliseconds(100));

    svr.stop();
    httpThread.join();
    reader.stop();
    return 0;
}
