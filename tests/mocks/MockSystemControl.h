#pragma once
#include "loki/agents/ISystemControl.h"
#include <string>

namespace loki::testing {
    /**
     * @class MockSystemControl
     * @brief A mock implementation of ISystemControl for testing the SystemControlAgent.
     */
    class MockSystemControl : public loki::agents::ISystemControl {
    public:
        // --- Spies to record calls ---
        std::string last_app_launched;
        std::string last_volume_direction;
        int last_volume_value = -1;
        std::string last_power_state;
        int call_count = 0;

        // --- Control knob for tests ---
        bool should_succeed = true;

        // --- ISystemControl implementation ---
        bool launchApplication(const std::string &appName) override {
            call_count++;
            last_app_launched = appName;
            return should_succeed;
        }

        bool setVolumeDirection(const std::string &direction) override {
            call_count++;
            last_volume_direction = direction;
            return should_succeed;
        }

        bool setVolumeValue(int value) override {
            call_count++;
            last_volume_value = value;
            return should_succeed;
        }

        bool changePowerState(const std::string &state) override {
            call_count++;
            last_power_state = state;
            return should_succeed;
        }

        // --- Test utility ---
        void reset() {
            last_app_launched.clear();
            last_volume_direction.clear();
            last_volume_value = -1;
            last_power_state.clear();
            call_count = 0;
            should_succeed = true;
        }
    };
} // namespace loki::testing
