#include <gtest/gtest.h>

// This entire test file will only be compiled on Windows
#ifdef _WIN32

#include "loki/agents/SystemControlAgent.h"
#include <memory>

// Windows-specific headers for checking system state
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <iostream>

// Test fixture for System Control Integration Tests
// This fixture handles the setup and teardown of Windows COM objects
// and ensures the system's volume is restored to its original state after tests run.
class SystemControlIntegrationTest : public ::testing::Test {
protected:
    std::unique_ptr<SystemControlAgent> agent;
    IAudioEndpointVolume *pEndpointVolume = nullptr;

    // Store original state to restore it later
    float originalVolumeScalar;
    BOOL originalMute;

    // This method runs before each test
    void SetUp() override {
        // 1. Initialize the agent without any mocks
        agent = std::make_unique<SystemControlAgent>();

        // 2. Initialize COM for this thread to access system APIs
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        ASSERT_TRUE(SUCCEEDED(hr)) << "CoInitializeEx failed!";

        // 3. Get the default audio endpoint device
        IMMDeviceEnumerator *pEnumerator = nullptr;
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                              (void **) &pEnumerator);
        ASSERT_TRUE(SUCCEEDED(hr)) << "CoCreateInstance failed!";

        IMMDevice *pDevice = nullptr;
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        ASSERT_TRUE(SUCCEEDED(hr)) << "GetDefaultAudioEndpoint failed!";

        // 4. Activate the volume control interface
        hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void **) &pEndpointVolume);
        ASSERT_TRUE(SUCCEEDED(hr)) << "Device Activate for IAudioEndpointVolume failed!";

        // 5. Store the original volume and mute state
        pEndpointVolume->GetMasterVolumeLevelScalar(&originalVolumeScalar);
        pEndpointVolume->GetMute(&originalMute);

        // Cleanup intermediate objects
        if (pDevice) pDevice->Release();
        if (pEnumerator) pEnumerator->Release();
    }

    // This method runs after each test
    void TearDown() override {
        // Restore the original volume and mute state to not affect the user's system
        if (pEndpointVolume) {
            pEndpointVolume->SetMasterVolumeLevelScalar(originalVolumeScalar, nullptr);
            pEndpointVolume->SetMute(originalMute, nullptr);
            pEndpointVolume->Release();
        }
        CoUninitialize();
    }
};

// Test Case 1: Set volume to a specific value and verify
TEST_F(SystemControlIntegrationTest, SetsVolumeToSpecificValue) {
    // Arrange
    loki::intent::Intent intent;
    intent.action = "set_volume";
    intent.parameters["value"] = 65; // Use a non-standard value

    // Act
    std::string response = agent->execute(intent);
    
    // Assert
    EXPECT_EQ(response, "Okay, volume set to 65 percent.");

    float newVolumeScalar;
    pEndpointVolume->GetMasterVolumeLevelScalar(&newVolumeScalar);
    ASSERT_NEAR(newVolumeScalar, 0.65f, 0.01f);
}

// Test Case 2: Mute and then unmute the system audio
TEST_F(SystemControlIntegrationTest, MutesAndUnmutesSystem) {
    // Arrange: Mute
    loki::intent::Intent mute_intent;
    mute_intent.action = "set_volume";
    mute_intent.parameters["direction"] = "mute";

    // Act: Mute
    std::string mute_response = agent->execute(mute_intent);
    
    // Assert: Mute
    EXPECT_EQ(mute_response, "Muted.");
    BOOL isMuted;
    pEndpointVolume->GetMute(&isMuted);
    ASSERT_TRUE(isMuted);

    // Arrange: Unmute
    loki::intent::Intent unmute_intent;
    unmute_intent.action = "set_volume";
    unmute_intent.parameters["direction"] = "unmute";

    // Act: Unmute
    std::string unmute_response = agent->execute(unmute_intent);
    
    // Assert: Unmute
    EXPECT_EQ(unmute_response, "Unmuted.");
    pEndpointVolume->GetMute(&isMuted);
    ASSERT_FALSE(isMuted);
}

// Test Case 3: Step volume up and down
TEST_F(SystemControlIntegrationTest, StepsVolumeUpAndDown) {
    // Set a known starting point to avoid hitting 0% or 100%
    pEndpointVolume->SetMasterVolumeLevelScalar(0.5f, nullptr);
    float initialVolume;
    pEndpointVolume->GetMasterVolumeLevelScalar(&initialVolume);

    // Arrange: Step Up
    loki::intent::Intent up_intent;
    up_intent.action = "set_volume";
    up_intent.parameters["direction"] = "up";

    // Act: Step Up
    agent->execute(up_intent);
    
    // Assert: Step Up
    float volumeAfterUp;
    pEndpointVolume->GetMasterVolumeLevelScalar(&volumeAfterUp);
    ASSERT_GT(volumeAfterUp, initialVolume) << "Volume should have increased.";

    // Arrange: Step Down
    loki::intent::Intent down_intent;
    down_intent.action = "set_volume";
    down_intent.parameters["direction"] = "down";

    // Act: Step Down
    agent->execute(down_intent);
    
    // Assert: Step Down
    float finalVolume;
    pEndpointVolume->GetMasterVolumeLevelScalar(&finalVolume);
    // The volume should be back to (or very close to) where it started
    ASSERT_NEAR(finalVolume, initialVolume, 0.01f);
}

// Test Case 4: Test a safe application launch
// NOTE: We can't easily verify if the process actually started without making the
// test flaky. Instead, we verify that the agent returns a success message, which
// implies the underlying ShellExecuteA call succeeded.
TEST_F(SystemControlIntegrationTest, LaunchApplication_CheckResponseOnly) {
    // Arrange
    loki::intent::Intent intent;
    intent.action = "launch_application";
    intent.parameters["name"] = "notepad";

    // Act
    std::string response = agent->execute(intent);

    // Assert
    EXPECT_EQ(response, "Okay, launching notepad");
}

#endif // _WIN32
