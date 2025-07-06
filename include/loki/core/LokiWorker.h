#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <vector>
#include <string>
#include <QTimer> // ADDED: For non-blocking processing loop
#include <memory>

// C-API and Class Headers
extern "C" {
    #include "picovoice/include/pv_porcupine.h"
}
#include "loki/core/Whisper.h"              // ADDED: Include for the custom deleter

// Forward-declarations for other types
struct ma_device;
class Config;
class OllamaClient;
class IntentClassifier;
class FastClassifier;
class AgentManager;
class EmbeddingModel;
struct AppData;

// --- FIXED: Custom Deleter for Whisper ---
struct WhisperDeleter {
    void operator()(Whisper *w) const {
        if (w) {
            w->destroy();
        }
    }
};

class LokiWorker : public QObject {
    Q_OBJECT

public:
    explicit LokiWorker(QObject *parent = nullptr);

    ~LokiWorker() override;

public slots:
    void initialize();

    void start_processing();

    void stop_processing();

signals:
    void status_updated(const QString &message);

    void loki_response(const QString &message);

    void finished();

    void wake_word_detected_signal();

private slots: // ADDED: Make it a slot for the timer
    void check_for_command();

private:
    // --- Member Variables ---
    std::unique_ptr<Config> config_;
    std::unique_ptr<OllamaClient> ollama_client_;

    std::unique_ptr<EmbeddingModel> embedding_model_;
    // --- FIXED: Use the custom deleter for the Whisper unique_ptr ---
    std::unique_ptr<Whisper, WhisperDeleter> whisper_;
    std::unique_ptr<FastClassifier> fast_classifier_;
    std::unique_ptr<IntentClassifier> llm_classifier_;

    std::unique_ptr<AgentManager> agent_manager_;

    std::unique_ptr<AppData> app_data_;
    std::unique_ptr<ma_device> device_;
    pv_porcupine_t *porcupine_ = nullptr; // This is now okay because we included the header
    QTimer *processing_timer_ = nullptr; // ADDED: Timer for the processing loop
    int min_command_ms_{};
};
