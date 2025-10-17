#pragma once
#include <string>

namespace loki::agents {
    /**
     * @class ISystemControl
     * @brief An interface for abstracting OS-level system control operations.
     *
     * This allows for platform-specific implementations (e.g., for Windows, Linux)
     * and enables easy mocking for unit tests.
     */
    class ISystemControl {
    public:
        virtual ~ISystemControl() = default;

        /** @brief Launches a named application. */
        virtual bool launchApplication(const std::string &appName) = 0;

        /** @brief Adjusts volume up, down, or mutes/unmutes. */
        virtual bool setVolumeDirection(const std::string &direction) = 0;

        /** @brief Sets the volume to a specific percentage (0-100). */
        virtual bool setVolumeValue(int value) = 0;

        /** @brief Changes the system's power state (shutdown, restart, sleep, logoff). */
        virtual bool changePowerState(const std::string &state) = 0;
    };
} // namespace loki::agents