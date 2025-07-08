#ifndef LOKI_WHISPER_H
#define LOKI_WHISPER_H

#include <string>
#include <cstdint>
#include <vector>
#include <memory> // ADDED for std::unique_ptr

// This is our C++ wrapper class for the whisper.cpp C-style API.
// It uses the "PIMPL" (Pointer to Implementation) idiom to hide
// the underlying C-API details from the rest of our application.

class Whisper {
public:
    // Factory function to create and initialize a Whisper instance.
    // UPDATED: Returns a unique_ptr for automatic memory management.
    static std::unique_ptr<Whisper> create(const std::string &model_path);

    // UPDATED: Public destructor is required for std::unique_ptr.
    // Its definition is in the .cpp file to allow PIMPL with an incomplete type.
    ~Whisper();

    // Transcribe a chunk of audio.
    // Audio data must be 16kHz, 32-bit float, mono.
    std::string process_audio(const std::vector<float> &audio_data);

    // REMOVED: No longer needed, unique_ptr handles destruction.
    // void destroy();

private:
    // UPDATED: Private constructor to enforce usage of the `create` factory function.
    Whisper();

    // Opaque pointer to the actual implementation details.
    class WhisperImpl; // Forward declaration
    WhisperImpl *impl_ = nullptr;
};

#endif //LOKI_WHISPER_H
