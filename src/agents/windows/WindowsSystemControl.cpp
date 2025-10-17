#include "loki/agents/windows/WindowsSystemControl.h"
#include <iostream>
#include <string>
#include <algorithm> // for std::clamp

// --- Windows Specific Headers ---
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>       // For ShellExecuteA
#include <mmdeviceapi.h>    // For modern volume control
#include <endpointvolume.h> // For modern volume control
#include <powrprof.h>       // For system sleep

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "PowrProf.lib")

namespace loki::agents {
    namespace {
        // Anonymous namespace for local helpers

        // RAII wrapper for COM initialization
        struct ComInitializer {
            ComInitializer() {
                // S_FALSE means it's already initialized, which is not an error for us.
                if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
                     std::cerr << "SYS_CONTROL_ERROR: CoInitializeEx failed!" << std::endl;
                }
            }
            // The corresponding CoUninitialize will be called by a singleton-like
            // object when the thread terminates.
        };

        // This ensures COM is initialized only once per thread and uninitialized on thread exit.
        thread_local ComInitializer com_initializer_for_this_thread;

        // Helper function to enable the SE_SHUTDOWN_NAME privilege
        bool enable_shutdown_privilege() {
            HANDLE hToken;
            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
                std::cerr << "SYS_CONTROL_ERROR: OpenProcessToken failed." << std::endl;
                return false;
            }

            LUID luid;
            if (!LookupPrivilegeValue(nullptr, SE_SHUTDOWN_NAME, &luid)) {
                std::cerr << "SYS_CONTROL_ERROR: LookupPrivilegeValue failed." << std::endl;
                CloseHandle(hToken);
                return false;
            }

            TOKEN_PRIVILEGES tp;
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

            if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr)) {
                std::cerr << "SYS_CONTROL_ERROR: AdjustTokenPrivileges failed." << std::endl;
                CloseHandle(hToken);
                return false;
            }

            CloseHandle(hToken);
            return true;
        }
    } // anonymous namespace


    bool WindowsSystemControl::launchApplication(const std::string &appName) {
        std::string app_to_launch = appName;
        if (app_to_launch.rfind(".exe") == std::string::npos &&
            app_to_launch != "explorer" && app_to_launch != "browser") {
            app_to_launch += ".exe";
        }
        std::cout << "SYS_CONTROL_LOG: Attempting to launch '" << app_to_launch << "'..." << std::endl;
        HINSTANCE result = ShellExecuteA(nullptr, "open", app_to_launch.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return (intptr_t) result > 32;
    }

    bool WindowsSystemControl::setVolumeDirection(const std::string &direction) {
        IMMDeviceEnumerator *pEnumerator = nullptr;
        IMMDevice *pDevice = nullptr;
        IAudioEndpointVolume *pEndpointVolume = nullptr;
        bool success = false;

        if (SUCCEEDED(
            CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **)
                &pEnumerator)) &&
            SUCCEEDED(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice)) &&
            SUCCEEDED(
                pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void **)&pEndpointVolume))) {
            if (direction == "up") success = SUCCEEDED(pEndpointVolume->VolumeStepUp(nullptr));
            else if (direction == "down") success = SUCCEEDED(pEndpointVolume->VolumeStepDown(nullptr));
            else if (direction == "mute") success = SUCCEEDED(pEndpointVolume->SetMute(TRUE, nullptr));
            else if (direction == "unmute") success = SUCCEEDED(pEndpointVolume->SetMute(FALSE, nullptr));
        }

        if (pEndpointVolume) pEndpointVolume->Release();
        if (pDevice) pDevice->Release();
        if (pEnumerator) pEnumerator->Release();

        return success;
    }

    bool WindowsSystemControl::setVolumeValue(int value) {
        IMMDeviceEnumerator *pEnumerator = nullptr;
        IMMDevice *pDevice = nullptr;
        IAudioEndpointVolume *pEndpointVolume = nullptr;
        bool success = false;

        if (SUCCEEDED(
            CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **)
                &pEnumerator)) &&
            SUCCEEDED(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice)) &&
            SUCCEEDED(
                pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void **)&pEndpointVolume))) {
            float new_volume = std::clamp(static_cast<float>(value) / 100.0f, 0.0f, 1.0f);
            success = SUCCEEDED(pEndpointVolume->SetMasterVolumeLevelScalar(new_volume, nullptr));
        }

        if (pEndpointVolume) pEndpointVolume->Release();
        if (pDevice) pDevice->Release();
        if (pEnumerator) pEnumerator->Release();

        return success;
    }

    bool WindowsSystemControl::changePowerState(const std::string &state) {
        if (state == "shutdown" || state == "restart") {
            if (!enable_shutdown_privilege()) {
                return false;
            }
            UINT flags = (state == "shutdown") ? EWX_SHUTDOWN : EWX_REBOOT;
            return ExitWindowsEx(flags | EWX_FORCEIFHUNG, SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_FLAG_PLANNED);
        } else if (state == "sleep") {
            return SetSuspendState(FALSE, TRUE, TRUE);
        } else if (state == "logoff") {
            return ExitWindowsEx(EWX_LOGOFF, 0);
        }
        return false;
    }
} // namespace loki::agents
