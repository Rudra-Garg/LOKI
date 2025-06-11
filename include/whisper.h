#ifndef LOKI_WHISPER_H
#define LOKI_WHISPER_H

#include <string>
#include <cstdint>
#include <vector>

// This is our C++ wrapper class for the whisper.cpp C-style API.
// It uses the "PIMPL" (Pointer to Implementation) idiom to hide
// the underlying C-API details from the rest of our application.

class Whisper {
public:
    // Factory function to create and initialize a Whisper instance.
    static Whisper *create(const std::string &model_path);

    // Transcribe a chunk of audio.
    // Audio data must be 16kHz, 32-bit float, mono.
    std::string process_audio(const std::vector<float> &audio_data);

    // Clean up all resources used by this instance.
    void destroy();

private:
    // Private constructor and destructor to enforce usage of create/destroy
    Whisper();

    ~Whisper();

    // Opaque pointer to the actual implementation details.
    class WhisperImpl; // Forward declaration
    WhisperImpl *impl_ = nullptr;
};

#endif //LOKI_WHISPER_H
