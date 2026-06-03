// Windows Service entry point for BatchMixerSCADA.
// Compiled only on Windows (see CMakeLists.txt if(WIN32) guard).
//
// Install:   sc create BatchMixerSCADA binPath= "C:\BatchMixer\batch_mixer_service.exe"
//            sc.exe failure BatchMixerSCADA reset= 86400 actions= restart/5000/restart/5000/reboot/5000
// Start:     sc start BatchMixerSCADA
// Stop:      sc stop BatchMixerSCADA
// Uninstall: sc delete BatchMixerSCADA

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <format>
#include <string>
#include <thread>
#include <atomic>

#include "BatchController.h"
#include "SimulatedAdsLink.h"

namespace {

constexpr wchar_t kServiceName[] = L"BatchMixerSCADA";

SERVICE_STATUS        g_status{};
SERVICE_STATUS_HANDLE g_handle{};
std::atomic<bool>     g_running{false};

HANDLE g_evt_source{nullptr};

void LogEvent(WORD type, const std::string& msg) {
    if (!g_evt_source) return;
    const char* strs[] = {msg.c_str()};
    ReportEventA(g_evt_source, type, 0, 0, nullptr, 1, 0, strs, nullptr);
}

void SetStatus(DWORD state, DWORD exit_code = NO_ERROR) {
    g_status.dwCurrentState  = state;
    g_status.dwWin32ExitCode = exit_code;
    SetServiceStatus(g_handle, &g_status);
}

DWORD WINAPI HandlerEx(DWORD ctrl, DWORD, LPVOID, LPVOID) {
    switch (ctrl) {
    case SERVICE_CONTROL_STOP:
        SetStatus(SERVICE_STOP_PENDING);
        g_running.store(false);
        return NO_ERROR;
    case SERVICE_CONTROL_INTERROGATE:
        SetServiceStatus(g_handle, &g_status);
        return NO_ERROR;
    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

void WINAPI ServiceMain(DWORD, LPWSTR*) {
    g_status.dwServiceType             = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwControlsAccepted        = SERVICE_ACCEPT_STOP;
    g_status.dwCurrentState            = SERVICE_START_PENDING;
    g_handle = RegisterServiceCtrlHandlerExW(kServiceName, HandlerEx, nullptr);
    if (!g_handle) return;

    g_evt_source = RegisterEventSourceW(nullptr, kServiceName);

    // Construct hardware simulation objects
    Valve          silo_a{"silo_a"}, silo_b{"silo_b"}, discharge{"discharge"};
    WeighingScale  scale_a{"scale_a"}, scale_b{"scale_b"};
    MixerDrum      drum{"drum"};
    PressureSensor pressure{"pressure"};
    SimulatedAdsLink ads;

    BatchController ctrl{silo_a, silo_b, discharge, scale_a, scale_b,
                         drum, pressure, ads};

    RecipeConfig recipe{.id=1, .name="ConcreteC25", .material_a_kg=50.0,
                        .material_b_kg=30.0, .mix_duration_s=120,
                        .target_rpm=45, .max_pressure_bar=4.5};
    ctrl.loadRecipe(recipe);

    SetStatus(SERVICE_RUNNING);
    LogEvent(EVENTLOG_INFORMATION_TYPE,
             std::format("BatchMixerSCADA started — recipe '{}'", recipe.name));

    g_running.store(true);
    ctrl.start();

    while (g_running.load()) {
        BatchState s = ctrl.step(0.5);
        if (s == BatchState::EMERGENCY_STOP) {
            LogEvent(EVENTLOG_ERROR_TYPE,
                std::format("EMERGENCY_STOP — pressure trip"));
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    LogEvent(EVENTLOG_INFORMATION_TYPE, "BatchMixerSCADA stopping");
    SetStatus(SERVICE_STOPPED);
    if (g_evt_source) DeregisterEventSource(g_evt_source);
}

} // namespace

int wmain() {
    SERVICE_TABLE_ENTRYW table[] = {
        {const_cast<wchar_t*>(kServiceName), ServiceMain},
        {nullptr, nullptr}
    };
    StartServiceCtrlDispatcherW(table);
    return 0;
}

#else
int main() { return 0; }  // non-Windows stub
#endif
