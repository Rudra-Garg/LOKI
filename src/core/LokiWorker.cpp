#include "loki/core/LokiWorker.h"

// --- Standard Library and Third-Party Includes ---
#include <chrono>
#include <thread>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <cmath> // Required for std::sqrt

#include "nlohmann/json.hpp"
#include "loki/core/Config.h"
#include "loki/core/OllamaClient.h"
#include "loki/core/Whisper.h"
#include "loki/core/EmbeddingModel.h"
#include "loki/intent/FastClassifier.h"
#include "loki/intent/IntentClassifier.h"
#include "loki/AgentManager.h"
#include "loki/agents/SystemControlAgent.h"
#include "loki/agents/CalculationAgent.h"

// --- C-API Headers ---
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"
#include "picovoice/include/pv_porcupine.h"


// --- Application State Structures and Callbacks ---
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
    float vad_threshold = 0.01f;
    LokiWorker *worker = nullptr;
};

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
    auto *pData = static_cast<AppData *>(pDevice->pUserData);
    if (pData == nullptr) return;
    bool wake_word_was_detected = false;
    std::lock_guard<std::mutex> lock(pData->mtx);
    const auto *samples_f32 = static_cast<const float *>(pInput);

    if (pData->state == AppState::RECORDING_COMMAND) {
        double sum_squares = 0.0;
        for (size_t i = 0; i < frameCount; ++i) { sum_squares += samples_f32[i] * samples_f32[i]; }
        double rms = std::sqrt(sum_squares / frameCount);

        pData->command_buffer.insert(pData->command_buffer.end(), samples_f32, samples_f32 + frameCount);
        if (rms < pData->vad_threshold) {
            pData->consecutive_silent_frames++;
        } else {
            pData->has_started_speaking = true;
            pData->consecutive_silent_frames = 0;
        }

        constexpr int SILENT_FRAMES_AFTER_SPEECH = 40;
        constexpr int SILENT_FRAMES_NO_SPEECH = 100;
        if ((pData->has_started_speaking && pData->consecutive_silent_frames > SILENT_FRAMES_AFTER_SPEECH) ||
            (!pData->has_started_speaking && pData->consecutive_silent_frames > SILENT_FRAMES_NO_SPEECH)) {
            pData->state = AppState::PROCESSING_COMMAND;
        }
    } else if (pData->state == AppState::LISTENING_FOR_WAKE_WORD) {
        pData->porcupine_buffer.resize(frameCount);
        for (ma_uint32 i = 0; i < frameCount; ++i) {
            pData->porcupine_buffer[i] = static_cast<int16_t>(samples_f32[i] * 32767.0f);
        }
        int32_t keyword_index;
        pv_porcupine_process(pData->porcupine, pData->porcupine_buffer.data(), &keyword_index);
        if (keyword_index != -1) {
            pData->state = AppState::RECORDING_COMMAND;
            pData->command_buffer.clear();
            pData->consecutive_silent_frames = 0;
            pData->has_started_speaking = false;
            wake_word_was_detected = true;
        }
    }
    if (wake_word_was_detected) {
        QMetaObject::invokeMethod(pData->worker, "wake_word_detected_signal", Qt::QueuedConnection);
    }
}


// --- LokiWorker Method Implementations ---

LokiWorker::LokiWorker(QObject *parent) : QObject(parent) {
    config_ = std::make_unique<Config>();
    app_data_ = std::make_unique<AppData>();
    device_ = std::make_unique<ma_device>();
    agent_manager_ = std::make_unique<AgentManager>();
    app_data_->worker = this;

    processing_timer_ = new QTimer(this);
    connect(processing_timer_, &QTimer::timeout, this, &LokiWorker::check_for_command);
}

LokiWorker::~LokiWorker() {
    stop_processing();
    if (device_ && device_->pUserData) {
        // Check if device was initialized
        ma_device_uninit(device_.get());
    }
    if (porcupine_) {
        pv_porcupine_delete(porcupine_);
    }
    std::cout << "LokiWorker destroyed." << std::endl;
}

void LokiWorker::initialize() {
    emit status_updated("Initializing...");
    const std::string ACCESS_KEY = config_->get("ACCESS_KEY", "");
    if (ACCESS_KEY.empty()) {
        emit status_updated("ERROR: ACCESS_KEY is not set in .env file!");
        return;
    }

    const std::string PORCUPINE_MODEL_PATH = config_->get("PORCUPINE_MODEL_PATH");
    const std::string KEYWORD_PATH = config_->get("KEYWORD_PATH");
    const std::string WHISPER_MODEL_PATH = config_->get("WHISPER_MODEL_PATH");
    const std::string EMBEDDING_MODEL_PATH = config_->get("EMBEDDING_MODEL_PATH");
    const std::string INTENTS_JSON_PATH = config_->get("INTENTS_JSON_PATH");

    const float SENSITIVITY = config_->get_float("SENSITIVITY", 0.5f);
    min_command_ms_ = std::stoi(config_->get("MIN_COMMAND_MS", "300"));
    app_data_->vad_threshold = config_->get_float("VAD_THRESHOLD", 0.01f);
    const std::string OLLAMA_HOST = config_->get("OLLAMA_HOST", "http://localhost:11434");
    const std::string OLLAMA_MODEL = config_->get("OLLAMA_MODEL", "dolphin-phi");

    emit status_updated("Initializing Porcupine...");
    const char *keyword_path_c_str = KEYWORD_PATH.c_str();
    pv_status_t porcupine_status = pv_porcupine_init(ACCESS_KEY.c_str(), PORCUPINE_MODEL_PATH.c_str(), 1,
                                                     &keyword_path_c_str, &SENSITIVITY, &porcupine_);
    if (porcupine_status != PV_STATUS_SUCCESS) {
        emit status_updated(QString("Porcupine init failed: %1").arg(pv_status_to_string(porcupine_status)));
        return;
    }
    app_data_->porcupine = porcupine_;

    emit status_updated("Initializing Whisper...");
    whisper_.reset(Whisper::create(WHISPER_MODEL_PATH));
    if (!whisper_) {
        emit status_updated("ERROR: Failed to load Whisper model!");
        return;
    }
    app_data_->whisper = whisper_.get();

    emit status_updated("Initializing Embedding Model...");
    embedding_model_ = EmbeddingModel::create(EMBEDDING_MODEL_PATH);
    if (!embedding_model_) {
        emit status_updated("CRITICAL: Could not create embedding model.");
        return;
    }

    emit status_updated("Initializing Classifiers...");
    fast_classifier_ = std::make_unique<FastClassifier>(INTENTS_JSON_PATH, *embedding_model_);
    nlohmann::json llm_options = {
        {"num_ctx", 1024},
        {"temperature", 0.0}, // zero randomness
        {"top_k", 1}, // only the single highest‐prob token
        {"top_p", 1.0}, // no nucleus sampling
        {"max_new_tokens", 128} // cap based on your longest JSON response
    };
    ollama_client_ = std::make_unique<OllamaClient>(OLLAMA_HOST, OLLAMA_MODEL, llm_options);
    llm_classifier_ = std::make_unique<IntentClassifier>(*ollama_client_);

    agent_manager_->register_agent(std::make_unique<SystemControlAgent>());
    agent_manager_->register_agent(std::make_unique<CalculationAgent>());

    emit status_updated("Initializing Audio Device...");
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format = ma_format_f32;
    deviceConfig.capture.channels = 1;
    deviceConfig.sampleRate = pv_sample_rate();
    deviceConfig.periodSizeInFrames = pv_porcupine_frame_length();
    deviceConfig.dataCallback = data_callback;
    deviceConfig.pUserData = app_data_.get();

    if (ma_device_init(nullptr, &deviceConfig, device_.get()) != MA_SUCCESS) {
        emit status_updated("ERROR: Failed to initialize capture device.");
        return;
    }

    emit status_updated(QString("Initialization complete. Using device: %1").arg(device_->capture.name));
}

void LokiWorker::start_processing() {
    if (ma_device_start(device_.get()) != MA_SUCCESS) {
        emit status_updated("ERROR: Failed to start capture device.");
        return;
    }
    processing_timer_->start(50); // Start the 50ms polling timer
    emit status_updated("Waiting for wake word ('Hey Loki')...");
}

void LokiWorker::stop_processing() {
    if (processing_timer_->isActive()) {
        processing_timer_->stop();
    }
}

void LokiWorker::check_for_command() {
    std::vector<float> audio_to_process;
    bool ready_to_process = false; {
        std::lock_guard<std::mutex> lock(app_data_->mtx);
        if (app_data_->state == AppState::PROCESSING_COMMAND) {
            ready_to_process = true;
            audio_to_process = std::move(app_data_->command_buffer);
            app_data_->state = AppState::LISTENING_FOR_WAKE_WORD;
        }
    }

    if (ready_to_process) {
        emit status_updated("Silence detected, processing...");
        const int audio_ms = static_cast<int>(audio_to_process.size() * 1000 / pv_sample_rate());

        if (audio_ms > min_command_ms_) {
            std::string transcription = app_data_->whisper->process_audio(audio_to_process);
            if (transcription.empty()) {
                emit status_updated("Heard nothing.");
            } else {
                emit status_updated(QString("Heard: \"%1\"").arg(QString::fromStdString(transcription)));
                auto fast_result = fast_classifier_->classify(transcription);
                intent::Intent intent;

                if (fast_result.has_match && fast_result.confidence >= 0.95f) {
                    emit status_updated("Fast path hit! Routing directly.");
                    intent = {fast_result.type, fast_result.action, fast_result.parameters, fast_result.confidence};
                } else {
                    emit status_updated("Fast path miss. Falling back to LLM...");
                    intent = llm_classifier_->classify(transcription);
                }

                if (intent.confidence >= 0.7) {
                    std::string response = agent_manager_->dispatch(intent);
                    emit loki_response(QString::fromStdString(response));
                } else {
                    emit loki_response("I'm not very confident about that. Could you please rephrase?");
                }
            }
        } else {
            emit status_updated(QString("Command too short (%1ms).").arg(audio_ms));
        }
    }
}
