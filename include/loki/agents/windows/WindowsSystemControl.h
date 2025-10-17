#pragma once
#include "loki/agents/ISystemControl.h"

namespace loki::agents {
    /**
     * @class WindowsSystemControl
     * @brief A Windows-specific implementation of the ISystemControl interface.
     *        Wraps all WinAPI calls for system operations.
     */
    class WindowsSystemControl : public ISystemControl {
    public:
        bool launchApplication(const std::string &appName) override;
        bool setVolumeDirection(const std::string &direction) override;
        bool setVolumeValue(int value) override;
        bool changePowerState(const std::string &state) override;
    };
} // namespace loki::agents