#include <gtest/gtest.h>
#include "loki/agents/SystemControlAgent.h"
#include "mocks/MockSystemControl.h"
#include <memory>

// Test fixture for the SystemControlAgent
class SystemControlAgentTest : public ::testing::Test {
protected:
    std::shared_ptr<loki::testing::MockSystemControl> mock_sys_control;
    std::unique_ptr<SystemControlAgent> agent;

    void SetUp() override {
        // Create the mock and inject it into the agent for each test.
        mock_sys_control = std::make_shared<loki::testing::MockSystemControl>();
        agent = std::make_unique<SystemControlAgent>(mock_sys_control);
    }
};

TEST_F(SystemControlAgentTest, HandlesLaunchApplicationSuccess) {
    loki::intent::Intent intent;
    intent.action = "launch_application";
    intent.parameters["name"] = "notepad";

    std::string result = agent->execute(intent);

    EXPECT_EQ(mock_sys_control->call_count, 1);
    EXPECT_EQ(mock_sys_control->last_app_launched, "notepad");
    EXPECT_EQ(result, "Okay, launching notepad");
}

TEST_F(SystemControlAgentTest, HandlesLaunchApplicationFailure) {
    mock_sys_control->should_succeed = false;
    loki::intent::Intent intent;
    intent.action = "launch_application";
    intent.parameters["name"] = "doomed_app";

    std::string result = agent->execute(intent);

    EXPECT_EQ(mock_sys_control->call_count, 1);
    EXPECT_EQ(mock_sys_control->last_app_launched, "doomed_app");
    EXPECT_EQ(result, "I'm sorry, I couldn't launch doomed_app");
}

TEST_F(SystemControlAgentTest, HandlesSetVolumeUp) {
    loki::intent::Intent intent;
    intent.action = "set_volume";
    intent.parameters["direction"] = "up";

    std::string result = agent->execute(intent);
    EXPECT_EQ(mock_sys_control->last_volume_direction, "up");
    EXPECT_EQ(result, "Volume increased.");
}

TEST_F(SystemControlAgentTest, HandlesSetVolumeFailure) {
    mock_sys_control->should_succeed = false;
    loki::intent::Intent intent;
    intent.action = "set_volume";
    intent.parameters["direction"] = "up";

    std::string result = agent->execute(intent);
    EXPECT_EQ(mock_sys_control->last_volume_direction, "up");
    EXPECT_EQ(result, "I couldn't adjust the volume.");
}

TEST_F(SystemControlAgentTest, HandlesSetVolumeToValue) {
    loki::intent::Intent intent;
    intent.action = "set_volume";
    intent.parameters["value"] = 75;

    std::string result = agent->execute(intent);
    EXPECT_EQ(mock_sys_control->last_volume_value, 75);
    EXPECT_EQ(result, "Okay, volume set to 75 percent.");
}

TEST_F(SystemControlAgentTest, HandlesPowerShutdownSuccess) {
    loki::intent::Intent intent;
    intent.action = "system_power";
    intent.parameters["power_state"] = "shutdown";

    std::string result = agent->execute(intent);
    EXPECT_EQ(mock_sys_control->last_power_state, "shutdown");
    EXPECT_EQ(result, "Shutting down now.");
}

TEST_F(SystemControlAgentTest, HandlesPowerSleepSuccess) {
    loki::intent::Intent intent;
    intent.action = "system_power";
    intent.parameters["power_state"] = "sleep";

    std::string result = agent->execute(intent);
    EXPECT_EQ(mock_sys_control->last_power_state, "sleep");
    EXPECT_EQ(result, "Entering sleep mode.");
}

TEST_F(SystemControlAgentTest, HandlesPowerFailure) {
    mock_sys_control->should_succeed = false;
    loki::intent::Intent intent;
    intent.action = "system_power";
    intent.parameters["power_state"] = "restart";

    std::string result = agent->execute(intent);
    EXPECT_EQ(mock_sys_control->last_power_state, "restart");
    EXPECT_EQ(result, "I couldn't get the required permissions to do that.");
}

TEST_F(SystemControlAgentTest, HandlesUnknownSystemAction) {
    loki::intent::Intent intent;
    intent.action = "make_coffee"; // Invalid action

    std::string result = agent->execute(intent);
    EXPECT_EQ(mock_sys_control->call_count, 0); // No mock methods should be called
    EXPECT_EQ(result, "I don't know how to perform that system control action.");
}