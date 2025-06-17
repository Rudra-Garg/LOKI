#include "loki/agents/SystemControlAgent.h"
#include <iostream>
#include <windows.h> // Required for ShellExecuteA on Windows

std::string SystemControlAgent::get_name() const {
    // This name MUST match the "type" from the IntentClassifier.
    return "system_control";
}

std::string SystemControlAgent::execute(const intent::Intent &intent) {
    if (intent.action == "launch_application") {
        if (!intent.parameters.contains("name")) {
            return "I can launch an application, but you need to tell me which one.";
        }
        std::string app_name = intent.parameters["name"];

        // Append .exe if it's not already there for robustness.
        if (app_name.rfind(".exe") == std::string::npos) {
            app_name += ".exe";
        }

        std::cout << "AGENT_LOG: Attempting to launch '" << app_name << "'..." << std::endl;

        // ShellExecuteA is a flexible way to launch applications on Windows.
        // It searches the system PATH, so you can launch "notepad.exe" or "chrome.exe" easily.
        HINSTANCE result = ShellExecuteA(
            NULL, // handle to parent window
            "open", // verb
            app_name.c_str(), // file to open
            NULL, // parameters for the file
            NULL, // default directory
            SW_SHOWNORMAL // show command
        );

        // According to Microsoft Docs, a return value > 32 indicates success.
        if ((intptr_t) result > 32) {
            // Success! Return a friendly confirmation message.
            return "Okay, launching " + intent.parameters["name"].get<std::string>();
        } else {
            // Failure. Return an error message.
            return "I'm sorry, I couldn't find or launch the application named " + intent.parameters["name"].get<
                       std::string>();
        }
    }
    // TODO: Add other system_control actions here, like "set_volume".

    return "I don't know how to perform that system control action.";
}
