#include "loki/agents/SystemControlAgent.h"
#include "loki/agents/ISystemControl.h"
#include <iostream>

// Platform-specific implementation is included only here.
// On other platforms, we could use #ifdefs to include a different implementation.
#ifdef _WIN32
#include "loki/agents/windows/WindowsSystemControl.h"
#endif

const std::string LOG_PREFIX = "[SystemControlAgent] ";

SystemControlAgent::SystemControlAgent(std::shared_ptr<loki::agents::ISystemControl> sys_control)
    : sys_control_(sys_control) {
    if (!sys_control_) {
        // If no implementation was injected (e.g., in production), create the default one.
#ifdef _WIN32
        sys_control_ = std::make_shared<loki::agents::WindowsSystemControl>();
#else
        // On non-windows platforms, this would either fail or use a stub.
        std::cerr << "WARNING: No system control implementation provided or available for this platform." << std::endl;
#endif
    }
}

std::string SystemControlAgent::get_name() const {
    return "system_control";
}

std::string SystemControlAgent::execute(const loki::intent::Intent &intent) {
    if (!sys_control_) {
        return "I'm sorry, system control is not available on this device.";
    }

    std::cout << LOG_PREFIX << "Executing action: '" << intent.action << "' with parameters: " << intent.parameters.dump() << std::endl;

    if (intent.action == "launch_application") {
        if (!intent.parameters.contains("name")) {
            std::cerr << LOG_PREFIX << "ERROR: 'launch_application' missing 'name' parameter." << std::endl;
            return "I can launch an application, but you need to tell me which one.";
        }
        std::string app_name = intent.parameters["name"];
        std::cout << LOG_PREFIX << "Calling sys_control_->launchApplication('" << app_name << "')..." << std::endl;
        bool success = sys_control_->launchApplication(app_name);
        std::cout << LOG_PREFIX << "...launchApplication returned: " << (success ? "true" : "false") << std::endl;

        if (success) {
            return "Okay, launching " + app_name;
        } else {
            return "I'm sorry, I couldn't launch " + app_name;
        }
    } else if (intent.action == "set_volume") {
        if (!intent.parameters.contains("direction") && !intent.parameters.contains("value")) {
            std::cout << LOG_PREFIX << "Missing 'direction' or 'value' parameter. Asking for clarification." << std::endl;
            return "Should I turn the volume up, down, or set it to a specific level?";
        }
        if (intent.parameters.contains("direction")) {
            std::string direction = intent.parameters["direction"];
            std::cout << LOG_PREFIX << "Calling sys_control_->setVolumeDirection('" << direction << "')..." << std::endl;
            bool success = sys_control_->setVolumeDirection(direction);
            std::cout << LOG_PREFIX << "...setVolumeDirection returned: " << (success ? "true" : "false") << std::endl;
            if (success) {
                if (direction == "up") return "Volume increased.";
                if (direction == "down") return "Volume decreased.";
                if (direction == "mute") return "Muted.";
                if (direction == "unmute") return "Unmuted.";
            }
        } else if (intent.parameters.contains("value")) {
            try {
                int value = intent.parameters["value"].get<int>();
                std::cout << LOG_PREFIX << "Calling sys_control_->setVolumeValue(" << value << ")..." << std::endl;
                bool success = sys_control_->setVolumeValue(value);
                std::cout << LOG_PREFIX << "...setVolumeValue returned: " << (success ? "true" : "false") << std::endl;
                if (success) {
                    return "Okay, volume set to " + std::to_string(value) + " percent.";
                }
            } catch (const nlohmann::json::exception& e) {
                std::cerr << LOG_PREFIX << "ERROR: Invalid 'value' parameter. " << e.what() << std::endl;
                return "That's not a valid volume level.";
            }
        }
        return "I couldn't adjust the volume.";
    } else if (intent.action == "system_power") {
        if (!intent.parameters.contains("power_state")) {
            std::cerr << LOG_PREFIX << "ERROR: 'system_power' missing 'power_state' parameter." << std::endl;
            return "I can shut down or restart, but you need to be specific.";
        }
        std::string state = intent.parameters["power_state"];

        std::cout << LOG_PREFIX << "Calling sys_control_->changePowerState('" << state << "')..." << std::endl;
        bool success = sys_control_->changePowerState(state);
        std::cout << LOG_PREFIX << "...changePowerState returned: " << (success ? "true" : "false") << std::endl;

        if (success) {
            if (state == "shutdown") return "Shutting down now.";
            if (state == "restart") return "Restarting now.";
            if (state == "sleep") return "Entering sleep mode.";
            if (state == "logoff") return "Logging off.";
        } else {
            std::cerr << LOG_PREFIX << "ERROR: System call to change power state failed. Permissions issue?" << std::endl;
            return "I couldn't get the required permissions to do that.";
        }
    }

    std::cerr << LOG_PREFIX << "WARNING: Unhandled action '" << intent.action << "'." << std::endl;
    return "I don't know how to perform that system control action.";
}
