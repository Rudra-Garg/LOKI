#include "loki/core/LokiWorker.h"

// --- Standard Library and Third-Party Includes ---
#include <chrono>
#include <thread>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <cmath> // Required for std::sqrt
#include <windows.h> // For SetEnvironmentVariableA
#include <QCoreApplication> // ADDED: For getting application path

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
#include "loki/tts/PiperTTS.h" // TTS header

// --- C-API Headers ---
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

extern "C" {
#include "picovoice/include/pv_porcupine.h"
}

// ADDED: Implementation for the custom deleter.
// This must be in the .cpp file where PiperTTS is a complete type.
void PiperTTSDeleter::operator()(loki::tts::PiperTTS *p) const {
    delete p;
}


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

// --- MINIAUDIO PLAYBACK ---
struct PlaybackData {
    ma_decoder decoder;
    std::atomic<bool> is_finished{false};
};

void playback_data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
    auto *pData = static_cast<PlaybackData *>(pDevice->pUserData);
    if (pData == nullptr) {
        return;
    }
    ma_uint64 frames_read;
    ma_result result = ma_decoder_read_pcm_frames(&pData->decoder, pOutput, frameCount, &frames_read);
    // If we fail to read or read less than requested, we're at the end.
    if (result != MA_SUCCESS || frames_read < frameCount) {
        pData->is_finished.store(true);
    }
    (void) pInput; // Not using input
}

// --- END MINIAUDIO PLAYBACK ---


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
    config_ = std::make_unique<loki::core::Config>();
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

    std::filesystem::path app_dir(QCoreApplication::applicationDirPath().toStdString());
    auto resolve_path = [&](const std::string &config_key, const std::string &default_val) -> std::string {
        std::string path_str = config_->get(config_key, default_val);
        std::filesystem::path p(path_str);
        if (p.is_absolute()) {
            if (std::filesystem::exists(p)) return p.string();
            std::cerr << "WARNING: Absolute path from config for '" << config_key << "' not found: " << p <<
                    ". Falling back to app directory." << std::endl;
            p = std::filesystem::path(config_->get(config_key, default_val));
        }
        return (app_dir / p).string();
    };

    const std::string ACCESS_KEY = config_->get("ACCESS_KEY", "");
    if (ACCESS_KEY.empty()) {
        emit status_updated("ERROR: ACCESS_KEY is not set in .env file!");
        emit initialization_complete();
        return;
    }

    const std::string PORCUPINE_MODEL_PATH = resolve_path("PORCUPINE_MODEL_PATH", "porcupine_params.pv");
    const std::string KEYWORD_PATH = resolve_path("KEYWORD_PATH", "Hey-Loki.ppn");
    const std::string WHISPER_MODEL_PATH = resolve_path("WHISPER_MODEL_PATH", "ggml-base.en.bin");
    const std::string EMBEDDING_MODEL_PATH = resolve_path("EMBEDDING_MODEL_PATH", "all-MiniLM-L6-v2.Q4_K_S.gguf");
    const std::string INTENTS_JSON_PATH = resolve_path("INTENTS_JSON_PATH", "intents.json");
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
        emit initialization_complete();
        return;
    }
    app_data_->porcupine = porcupine_;

    emit status_updated("Initializing Whisper...");
    whisper_.reset(Whisper::create(WHISPER_MODEL_PATH));
    if (!whisper_) {
        emit status_updated("ERROR: Failed to load Whisper model!");
        emit initialization_complete();
        return;
    }
    app_data_->whisper = whisper_.get();

    // ADDED: Detailed logging around TTS initialization
    std::cout << "LOKI_WORKER_LOG: About to initialize TTS..." << std::endl;
    emit status_updated("Initializing TTS...");
    std::string espeakDataAbsPath = resolve_path("ESPEAK_DATA_PATH", "espeak-ng-data");
    SetEnvironmentVariableA("ESPEAK_DATA_PATH", espeakDataAbsPath.c_str());
    std::string piper_exe_path = (app_dir / "piper.exe").string();
    std::string piper_model_path = resolve_path("PIPER_MODEL_PATH", "models/piper/en_US-hfc_male-medium.onnx");
    tts_.reset(new loki::tts::PiperTTS(piper_exe_path, piper_model_path, app_dir.string()));
    if (!tts_->initialize()) {
        std::string tts_error = tts_->getLastError();
        emit status_updated(QString("TTS Init Failed: %1").arg(QString::fromStdString(tts_error)));
        std::cout << "LOKI_WORKER_LOG: TTS initialization FAILED. Error: " << tts_error << std::endl;
    } else {
        emit status_updated("TTS initialized successfully.");
        std::cout << "LOKI_WORKER_LOG: TTS initialization SUCCEEDED." << std::endl;
    }
    std::cout << "LOKI_WORKER_LOG: Finished TTS initialization block." << std::endl;

    emit status_updated("Initializing Embedding Model...");
    embedding_model_ = EmbeddingModel::create(EMBEDDING_MODEL_PATH);
    if (!embedding_model_) {
        emit status_updated("CRITICAL: Could not create embedding model.");
        emit initialization_complete();
        return;
    }

    emit status_updated("Initializing Classifiers...");
    fast_classifier_ = std::make_unique<FastClassifier>(INTENTS_JSON_PATH, *embedding_model_);
    nlohmann::json llm_options = {
        {"num_ctx", 1024}, {"temperature", 0.0}, {"top_k", 1}, {"top_p", 1.0}, {"max_new_tokens", 128}
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
        emit initialization_complete();
        return;
    }

    emit status_updated(QString("Initialization complete. Using device: %1").arg(device_->capture.name));
    emit initialization_complete();
}

void LokiWorker::start_processing() {
    if (ma_device_start(device_.get()) != MA_SUCCESS) {
        emit status_updated("ERROR: Failed to start capture device.");
        return;
    }
    processing_timer_->start(50);
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

                auto handle_response = [&](const std::string &text) {
                    if (text.empty()) return;
                    emit loki_response(QString::fromStdString(text));
                    if (tts_ && tts_->isReady()) {
                        const std::string wav_path_relative = "response.wav";
                        if (tts_->synthesizeToFile(text, wav_path_relative)) {
                            play_audio(wav_path_relative);
                            std::filesystem::path wav_path_absolute = std::filesystem::path(
                                                                          QCoreApplication::applicationDirPath().
                                                                          toStdString()) / wav_path_relative;
                            if (std::filesystem::exists(wav_path_absolute)) {
                                std::filesystem::remove(wav_path_absolute);
                            }
                        } else {
                            emit status_updated(
                                QString("TTS Error: %1").arg(QString::fromStdString(tts_->getLastError())));
                        }
                    } else {
                        emit status_updated("TTS not ready, skipping playback.");
                    }
                };

                if (intent.confidence >= 0.7) {
                    std::string response = agent_manager_->dispatch(intent);
                    handle_response(response);
                } else {
                    handle_response("I'm not very confident about that. Could you please rephrase?");
                }
            }
        } else {
            emit status_updated(QString("Command too short (%1ms).").arg(audio_ms));
        }
    }
}

void LokiWorker::play_audio(const std::string &wav_path) {
    PlaybackData data;
    std::filesystem::path audio_file_path = wav_path;
    if (audio_file_path.is_relative()) {
        audio_file_path = std::filesystem::path(QCoreApplication::applicationDirPath().toStdString()) / wav_path;
    }
    if (!std::filesystem::exists(audio_file_path)) {
        emit status_updated(QString("Audio file not found for playback: %1").arg(audio_file_path.string().c_str()));
        return;
    }
    ma_decoder_config decoder_config = ma_decoder_config_init_default();
    ma_result result = ma_decoder_init_file(audio_file_path.string().c_str(), &decoder_config, &data.decoder);
    if (result != MA_SUCCESS) {
        emit status_updated(QString("Failed to open audio file: %1").arg(audio_file_path.string().c_str()));
        return;
    }
    ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
    device_config.playback.format = data.decoder.outputFormat;
    device_config.playback.channels = data.decoder.outputChannels;
    device_config.sampleRate = data.decoder.outputSampleRate;
    device_config.dataCallback = playback_data_callback;
    device_config.pUserData = &data;
    ma_device device;
    if (ma_device_init(NULL, &device_config, &device) != MA_SUCCESS) {
        emit status_updated("Failed to initialize playback device.");
        ma_decoder_uninit(&data.decoder);
        return;
    }
    if (ma_device_start(&device) != MA_SUCCESS) {
        emit status_updated("Failed to start playback device.");
        ma_device_uninit(&device);
        ma_decoder_uninit(&data.decoder);
        return;
    }
    emit status_updated("Playing response...");
    while (!data.is_finished.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ma_device_uninit(&device);
    ma_decoder_uninit(&data.decoder);
}
