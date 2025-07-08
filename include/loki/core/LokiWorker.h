#pragma once

#include <QObject>
#include <QTimer>
#include <memory>
#include <string>
#include <vector>

// Forward declarations
struct ma_device;
struct AppData;
struct pv_porcupine;
typedef struct pv_porcupine pv_porcupine_t;

namespace loki {
    namespace core {
        class Config;
        class OllamaClient;
    }

    namespace intent {
        class FastClassifier;
        class IntentClassifier;
    }

    namespace tts {
        class AsyncTTSManager;
        enum class TTSPriority;
    }
}

class Whisper;
class EmbeddingModel;
class AgentManager;

class LokiWorker : public QObject {
    Q_OBJECT

public:
    explicit LokiWorker(QObject *parent = nullptr);

    ~LokiWorker() override;

    // Core functionality
    void initialize();

    void start_processing();

    void stop_processing();

    // TTS functionality
    bool synthesize_text_sync(const QString &text, std::vector<char> &audioData, int timeoutMs = 5000);

    void speak_text_async(const QString &text, loki::tts::TTSPriority priority);

    // Audio playback
    void play_audio(const std::string &wav_path);

    void play_audio_from_memory(const std::vector<char> &audioData);

public slots:
    void check_for_command();

signals:
    void status_updated(const QString &status);

    void initialization_complete();

    void loki_response(const QString &response);

    void wake_word_detected_signal();

private:
    // Configuration and core components
    std::unique_ptr<loki::core::Config> config_;
    std::unique_ptr<AppData> app_data_;
    std::unique_ptr<ma_device> device_;
    QTimer *processing_timer_;

    // AI/ML components
    std::unique_ptr<Whisper> whisper_;
    std::unique_ptr<EmbeddingModel> embedding_model_;
    std::unique_ptr<loki::intent::FastClassifier> fast_classifier_;
    std::unique_ptr<loki::core::OllamaClient> ollama_client_;
    std::unique_ptr<loki::intent::IntentClassifier> llm_classifier_;
    std::unique_ptr<AgentManager> agent_manager_;

    // TTS system - UPDATED to use AsyncTTSManager
    std::unique_ptr<loki::tts::AsyncTTSManager> async_tts_;

    // Wake word detection
    pv_porcupine_t *porcupine_ = nullptr;

    // Configuration parameters
    int min_command_ms_ = 300;
};
