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

namespace loki {
    namespace core {
        class Config;
    }

    namespace tts {
        class PiperTTS;
    } // Forward-declare PiperTTS
}

class OllamaClient;
class IntentClassifier;
class FastClassifier;
class AgentManager;
class EmbeddingModel;
struct AppData;

// --- Custom Deleter for Whisper ---
struct WhisperDeleter {
    void operator()(Whisper *w) const {
        if (w) {
            w->destroy();
        }
    }
};

// --- ADDED: Custom Deleter for PiperTTS ---
// This is required because PiperTTS is forward-declared and its definition is
// hidden in the .cpp file (PIMPL idiom). The default deleter would require the
// full definition of PiperTTS here, which would break the encapsulation.
struct PiperTTSDeleter {
    void operator()(loki::tts::PiperTTS *p) const;
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

    void initialization_complete(); // ADDED: To signal when init is done

private slots: // ADDED: Make it a slot for the timer
    void check_for_command();

private:
    void play_audio(const std::string &wav_path); // For audio playback

    // --- Member Variables ---
    std::unique_ptr<loki::core::Config> config_;
    std::unique_ptr<OllamaClient> ollama_client_;
    // MODIFIED: Use the custom deleter for PiperTTS
    std::unique_ptr<loki::tts::PiperTTS, PiperTTSDeleter> tts_;

    std::unique_ptr<EmbeddingModel> embedding_model_;
    std::unique_ptr<Whisper, WhisperDeleter> whisper_;
    std::unique_ptr<FastClassifier> fast_classifier_;
    std::unique_ptr<IntentClassifier> llm_classifier_;

    std::unique_ptr<AgentManager> agent_manager_;

    std::unique_ptr<AppData> app_data_;
    std::unique_ptr<ma_device> device_;
    pv_porcupine_t *porcupine_ = nullptr;
    QTimer *processing_timer_ = nullptr;
    int min_command_ms_{};
};
