#pragma once

#include <string>
#include <memory>
#include <vector>

namespace loki::tts {
    class PiperTTS {
    public:
        // MODIFIED: Constructor now takes all necessary paths
        PiperTTS(const std::string &piperExePath, const std::string &modelPath, const std::string &appDirPath);

        ~PiperTTS();

        // MODIFIED: Simplified to just start the persistent process
        bool initialize();

        // Synthesize text to WAV file by communicating with the running process
        bool synthesizeToFile(const std::string &text, const std::string &outputWavPath);

        // Synthesize text and return as audio data
        bool synthesizeToMemory(const std::string &text, std::vector<char> &audioData);

        // Check if TTS process is running and ready
        bool isReady() const;

        // Get last error message
        std::string getLastError() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> pImpl;
    };
}
