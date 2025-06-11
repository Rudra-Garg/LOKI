#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <atomic>
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "pv_porcupine.h"
#include "whisper.h"
#include "Config.h"
#include "OllamaClient.h"
#include "intent/IntentClassifier.h"
#include "nlohmann/json.hpp"

// --- VAD Configuration ---
constexpr int SILENT_FRAMES_AFTER_SPEECH = 40;
constexpr int SILENT_FRAMES_NO_SPEECH = 100;

// --- Application State ---
enum class AppState {
    LISTENING_FOR_WAKE_WORD,
    RECORDING_COMMAND,
    PROCESSING_COMMAND
};

struct AppData {
    std::mutex mtx;
    pv_porcupine_t *porcupine = nullptr;
    Whisper *whisper = nullptr;
    AppState state = AppState::LISTENING_FOR_WAKE_WORD;
    std::vector<int16_t> porcupine_buffer;
    std::vector<float> command_buffer;
    int consecutive_silent_frames = 0;
    bool has_started_speaking = false;
    float vad_threshold;
};

bool ensure_directory_exists(const std::string &path) {
    try {
        if (!std::filesystem::exists(path)) {
            return std::filesystem::create_directories(path);
        }
        return true;
    } catch (const std::exception &e) {
        std::cerr << "Error creating directory " << path << ": " << e.what() << std::endl;
        return false;
    }
}

void save_to_wav(const std::string &filename, const std::vector<float> &audio_data, uint32_t sample_rate = 16000) {
    try {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "ERROR: Could not open file for writing: " << filename << std::endl;
            return;
        }

        // Convert float audio to 16-bit PCM
        std::vector<int16_t> pcm_data(audio_data.size());
        for (size_t i = 0; i < audio_data.size(); ++i) {
            float sample = std::max(-1.0f, std::min(1.0f, audio_data[i]));
            pcm_data[i] = static_cast<int16_t>(sample * 32767.0f);
        }

        // --- WAV Header ---
        const int num_channels = 1;
        const int bits_per_sample = 16;
        const int byte_rate = sample_rate * num_channels * bits_per_sample / 8;
        const int block_align = num_channels * bits_per_sample / 8;
        const int subchunk2_size = static_cast<int>(pcm_data.size() * num_channels * bits_per_sample / 8);
        const int chunk_size = 36 + subchunk2_size;

        // "RIFF" chunk descriptor
        file.write("RIFF", 4);
        file.write(reinterpret_cast<const char *>(&chunk_size), 4);
        file.write("WAVE", 4);

        // "fmt " sub-chunk
        file.write("fmt ", 4);
        int32_t subchunk1_size = 16;
        file.write(reinterpret_cast<const char *>(&subchunk1_size), 4);
        int16_t audio_format = 1; // PCM
        file.write(reinterpret_cast<const char *>(&audio_format), 2);
        int16_t channels = static_cast<int16_t>(num_channels);
        file.write(reinterpret_cast<const char *>(&channels), 2);
        file.write(reinterpret_cast<const char *>(&sample_rate), 4);
        file.write(reinterpret_cast<const char *>(&byte_rate), 4);
        int16_t block_align_16 = static_cast<int16_t>(block_align);
        file.write(reinterpret_cast<const char *>(&block_align_16), 2);
        int16_t bits_per_sample_16 = static_cast<int16_t>(bits_per_sample);
        file.write(reinterpret_cast<const char *>(&bits_per_sample_16), 2);

        // "data" sub-chunk
        file.write("data", 4);
        file.write(reinterpret_cast<const char *>(&subchunk2_size), 4);

        // Audio data
        file.write(reinterpret_cast<const char *>(pcm_data.data()), subchunk2_size);

        std::cout << "DEBUG: Saved audio to " << filename << " (" << audio_data.size() << " samples)" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "ERROR: Exception while saving WAV file: " << e.what() << std::endl;
    }
}

// --- VAD Helper Function ---
bool is_silent(const float *pcm_f32, size_t frame_count, float threshold) {
    if (frame_count == 0) return true;
    double sum_squares = 0.0;
    for (size_t i = 0; i < frame_count; ++i) {
        sum_squares += pcm_f32[i] * pcm_f32[i];
    }
    double rms = std::sqrt(sum_squares / frame_count);
    return rms < threshold;
}

// --- Audio Callback ---
void data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
    auto *pData = (AppData *) pDevice->pUserData;
    if (pData == nullptr) return;

    bool should_process = false;
    bool wake_word_detected = false;

    // --- Critical Section: Minimized and No I/O ---
    {
        std::lock_guard<std::mutex> lock(pData->mtx);
        const auto *samples_f32 = static_cast<const float *>(pInput);

        if (pData->state == AppState::RECORDING_COMMAND) {
            pData->command_buffer.insert(pData->command_buffer.end(), samples_f32, samples_f32 + frameCount);
            if (is_silent(samples_f32, frameCount, pData->vad_threshold)) {
                pData->consecutive_silent_frames++;
            } else {
                pData->has_started_speaking = true;
                pData->consecutive_silent_frames = 0;
            }
            if ((pData->has_started_speaking && pData->consecutive_silent_frames > SILENT_FRAMES_AFTER_SPEECH) ||
                (!pData->has_started_speaking && pData->consecutive_silent_frames > SILENT_FRAMES_NO_SPEECH)) {
                pData->state = AppState::PROCESSING_COMMAND;
                should_process = true;
            }
        } else if (pData->state == AppState::LISTENING_FOR_WAKE_WORD) {
            pData->porcupine_buffer.resize(frameCount);
            for (ma_uint32 i = 0; i < frameCount; ++i) {
                pData->porcupine_buffer[i] = (int16_t) (samples_f32[i] * 32767.0f);
            }
            int32_t keyword_index;
            pv_porcupine_process(pData->porcupine, pData->porcupine_buffer.data(), &keyword_index);
            if (keyword_index != -1) {
                pData->state = AppState::RECORDING_COMMAND;
                pData->command_buffer.clear();
                pData->consecutive_silent_frames = 0;
                pData->has_started_speaking = false;
                wake_word_detected = true;
            }
        }
    }
    // --- End of Critical Section ---

    // --- Logging Section: Performed safely outside the lock ---
    if (wake_word_detected) {
        std::cout << "\n\n!!! WAKE WORD DETECTED !!!" << std::endl;
        std::cout << ">>> Listening for command..." << std::endl;
    }
    if (should_process) {
        std::cout << "\n--- Silence detected, processing command... ---" << std::endl;
    }
}

// --- Main Function ---
int main() {
    Config config;
    const std::string ACCESS_KEY = config.get("ACCESS_KEY");
    const std::string PORCUPINE_MODEL_PATH = config.get("PORCUPINE_MODEL_PATH");
    const std::string KEYWORD_PATH = config.get("KEYWORD_PATH");
    const std::string WHISPER_MODEL_PATH = config.get("WHISPER_MODEL_PATH");
    const float SENSITIVITY = config.get_float("SENSITIVITY", 0.5f);
    const int MIN_COMMAND_MS = std::stoi(config.get("MIN_COMMAND_MS", "300"));
    const float VAD_THRESHOLD = config.get_float("VAD_THRESHOLD", 0.01f);
    const std::string OLLAMA_HOST = config.get("OLLAMA_HOST", "http://localhost:11434");
    const std::string OLLAMA_MODEL = config.get("OLLAMA_MODEL", "tinylama");

    AppData app_data;
    app_data.vad_threshold = VAD_THRESHOLD;

    if (ACCESS_KEY.empty()) {
        std::cerr << "ERROR: Please set your ACCESS_KEY in the .env file." << std::endl;
        return -1;
    }

    // Ensure debug directory exists
    if (!ensure_directory_exists("debug_audio")) {
        std::cerr << "WARNING: Could not create debug_audio directory. Audio files will not be saved." << std::endl;
    }

    const char *keyword_path_c_str = KEYWORD_PATH.c_str();
    pv_status_t porcupine_status = pv_porcupine_init(ACCESS_KEY.c_str(), PORCUPINE_MODEL_PATH.c_str(), 1,
                                                     &keyword_path_c_str, &SENSITIVITY, &app_data.porcupine);
    if (porcupine_status != PV_STATUS_SUCCESS) {
        std::cerr << "Failed to initialize Porcupine: " << pv_status_to_string(porcupine_status) << std::endl;
        return -1;
    }
    std::cout << "Porcupine initialized successfully." << std::endl;

    app_data.whisper = Whisper::create(WHISPER_MODEL_PATH);
    if (!app_data.whisper) {
        std::cerr << "Failed to load Whisper model at: " << WHISPER_MODEL_PATH << std::endl;
        return -1;
    }
    std::cout << "Whisper initialized successfully." << std::endl;

    // --- New Architecture Setup ---
    OllamaClient ollama_client(OLLAMA_HOST, OLLAMA_MODEL);
    IntentClassifier intent_classifier(ollama_client);
    // ---

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format = ma_format_f32;
    deviceConfig.capture.channels = 1;
    deviceConfig.sampleRate = pv_sample_rate();
    deviceConfig.periodSizeInFrames = pv_porcupine_frame_length();
    deviceConfig.dataCallback = data_callback;
    deviceConfig.pUserData = &app_data;

    ma_device device;
    if (ma_device_init(nullptr, &deviceConfig, &device) != MA_SUCCESS) {
        std::cerr << "Failed to initialize capture device." << std::endl;
        return -1;
    }
    if (ma_device_start(&device) != MA_SUCCESS) {
        std::cerr << "Failed to start capture device." << std::endl;
        ma_device_uninit(&device);
        return -1;
    }
    std::cout << "Using device: " << device.capture.name << std::endl;
    std::cout << "\n<<< Waiting for wake word ('Hey Loki')..." << std::endl;

    // Set up non-blocking input check
    std::atomic<bool> should_stop{false};
    std::thread input_thread([&should_stop]() {
        std::cout << "Press Enter to stop..." << std::endl;
        std::cin.get(); // Wait for any input
        should_stop = true;
    });

    while (!should_stop) {
        std::vector<float> audio_to_process;
        bool ready_to_process = false;

        // --- Critical Section: Check state and grab data ---
        {
            std::lock_guard<std::mutex> lock(app_data.mtx);
            if (app_data.state == AppState::PROCESSING_COMMAND) {
                audio_to_process = std::move(app_data.command_buffer);
                app_data.state = AppState::LISTENING_FOR_WAKE_WORD;
                ready_to_process = true;
            }
        }
        // --- End of Critical Section ---

        if (ready_to_process) {
            const int audio_ms = static_cast<int>(audio_to_process.size() / (pv_sample_rate() / 1000));
            std::cout << "DEBUG: Processing " << audio_ms << "ms of audio (" << audio_to_process.size() << " samples)."
                    << std::endl;

            if (audio_ms > MIN_COMMAND_MS) {
                std::string transcription = app_data.whisper->process_audio(audio_to_process);

                if (transcription.empty()) {
                    std::cout << ">>> Transcription: [EMPTY]" << std::endl;
                } else {
                    std::cout << ">>> Heard: " << transcription << std::endl;

                    // --- Classify the intent ---
                    IntentClassifier::Intent intent = intent_classifier.classify(transcription);

                    std::cout << "\n<<< Intent Recognized >>>" << std::endl;
                    std::cout << "  Type:       " << intent.type << std::endl;
                    std::cout << "  Action:     " << intent.action << std::endl;
                    std::cout << "  Confidence: " << std::fixed << std::setprecision(2) << intent.confidence <<
                            std::endl;
                    std::cout << "  Parameters: " << intent.parameters.dump(2) << std::endl;

                    // This is where the AgentManager will eventually go.
                    // For now, we can add a simple confidence check.
                    if (intent.confidence < 0.7) {
                        std::cout << "\nLOKI: I'm not very confident about that. Could you please rephrase?" <<
                                std::endl;
                    } else {
                        std::cout << "\nACTION: (Routing to agent for type '" << intent.type << "' would happen here)"
                                << std::endl;
                    }
                }
            } else {
                std::cout << ">>> No command recorded (audio too short: " << audio_ms << "ms < " << MIN_COMMAND_MS <<
                        "ms)." << std::endl;
            }
            std::cout << "\n<<< Waiting for wake word ('Hey Loki')..." << std::endl;
            std::cout.flush();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Wait for input thread to finish
    if (input_thread.joinable()) {
        input_thread.join();
    }

    // --- Cleanup ---
    std::cout << "\nShutting down..." << std::endl;
    ma_device_uninit(&device);
    if (app_data.whisper) {
        app_data.whisper->destroy();
    }
    pv_porcupine_delete(app_data.porcupine);
    std::cout << "Stopped." << std::endl;
    return 0;
}
