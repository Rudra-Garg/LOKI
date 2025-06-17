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
#include "miniaudio/miniaudio.h"
#include "picovoice/include/pv_porcupine.h"
#include "loki/core/Whisper.h"
#include "loki/core/Config.h"
#include "loki/core/OllamaClient.h"
#include "loki/intent/IntentClassifier.h"
#include "nlohmann/json.hpp"
#include "loki/intent/FastClassifier.h"

#include "loki/AgentManager.h"
#include "loki/agents/SystemControlAgent.h"
#include "loki/agents/CalculationAgent.h"

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
    const std::string OLLAMA_MODEL = config.get("OLLAMA_MODEL", "dolphin-phi");

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
    nlohmann::json llm_options = {
        {"num_ctx", 512}, // Reduce context size for speed
        {"temperature", 0.0}, // Make output deterministic
        {"top_k", 40} // Use only the most likely tokens
    };
    // --- New Architecture Setup ---
    OllamaClient ollama_client(OLLAMA_HOST, OLLAMA_MODEL, llm_options);
    // Initialize the fast classifier with the paths to the embedding model and its vocabulary
    // 1. Create the Embedding Model first.
    const std::string EMBEDDING_MODEL_PATH = "all-MiniLM-L6-v2.Q4_K_S.gguf";
    auto embedding_model = EmbeddingModel::create(EMBEDDING_MODEL_PATH);
    if (!embedding_model) {
        std::cerr << "CRITICAL: Could not create embedding model." << std::endl;
        return -1;
    }

    // 2. Now, create the FastClassifier, passing the model to it.
    const std::string INTENTS_JSON_PATH = "intents.json";
    FastClassifier fast_classifier(INTENTS_JSON_PATH, *embedding_model);

    // 3. The LLM Classifier is independent.
    IntentClassifier llm_classifier(ollama_client);

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
    AgentManager agent_manager;
    agent_manager.register_agent(std::make_unique<SystemControlAgent>());
    agent_manager.register_agent(std::make_unique<CalculationAgent>());
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
                    // --- STAGE 2: Fast Path Classification ---
                    auto fast_result = fast_classifier.classify(transcription);
                    intent::Intent intent; // The final intent to be acted upon

                    const float FAST_PATH_CONFIDENCE_THRESHOLD = 0.95f;

                    if (fast_result.has_match && fast_result.confidence >= FAST_PATH_CONFIDENCE_THRESHOLD) {
                        std::cout << "\n>>> Fast Path HIT! Routing directly." << std::endl;
                        intent.type = fast_result.type;
                        intent.action = fast_result.action;
                        intent.parameters = fast_result.parameters;
                        intent.confidence = fast_result.confidence;
                    } else {
                        std::cout << "\n>>> Fast Path MISS. Falling back to LLM." << std::endl;
                        // --- STAGE 3: LLM Fallback ---
                        intent = llm_classifier.classify(transcription);
                    }

                    std::cout << "\n<<< Intent Recognized >>>" << std::endl;
                    std::cout << "  Type:       " << intent.type << std::endl;
                    std::cout << "  Action:     " << intent.action << std::endl;
                    std::cout << "  Confidence: " << std::fixed << std::setprecision(2) << intent.confidence <<
                            std::endl;
                    std::cout << "  Parameters: " << intent.parameters.dump(2) << std::endl;

                    if (intent.confidence >= 0.7) {
                        std::string response = agent_manager.dispatch(intent);
                        std::cout << "\nLOKI says: \"" << response << "\"" << std::endl;
                        // Later, this response will be fed to a Text-to-Speech engine
                    } else {
                        // This part is already correct
                        std::cout << "\nLOKI: I'm not very confident about that. Could you please rephrase?" <<
                                std::endl;
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
