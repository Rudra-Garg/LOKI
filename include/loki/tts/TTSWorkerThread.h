#pragma once

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QString>
#include <QObject>
#include <memory>
#include <vector>
#include <atomic>
#include "PiperTTS.h"

namespace loki::tts {
    enum class TTSPriority {
        LOW = 0,
        NORMAL = 1,
        HIGH = 2,
        IMMEDIATE = 3
    };

    struct TTSRequest {
        QString text;
        TTSPriority priority;
        uint64_t requestId;
        bool cancelled = false;

        TTSRequest(const QString &txt, TTSPriority prio, uint64_t id)
            : text(txt), priority(prio), requestId(id) {
        }
    };

    struct TTSResponse {
        uint64_t requestId;
        bool success;
        std::vector<char> audioData;
        QString errorMessage;
        QString originalText;
    };

    class TTSWorkerThread : public QThread {
        Q_OBJECT

    public:
        explicit TTSWorkerThread(const std::string &piperExePath,
                                 const std::string &modelPath,
                                 const std::string &appDirPath,
                                 QObject *parent = nullptr);

        ~TTSWorkerThread() override;

        // Async TTS request - returns request ID
        uint64_t synthesizeAsync(const QString &text, TTSPriority priority = TTSPriority::NORMAL);

        // Cancel a specific request
        void cancelRequest(uint64_t requestId);

        // Cancel all pending requests
        void cancelAllRequests();

        // Check if TTS is ready
        bool isReady() const { return ttsReady_.load(); }

        // Get queue size
        int getQueueSize() const;

    signals:
        void synthesisCompleted(const TTSResponse &response);

        void ttsInitialized(bool success, const QString &errorMessage);

        void ttsError(const QString &errorMessage);

    protected:
        void run() override;

    private slots:
        void handleShutdown();

    private:
        void initializeTTS();

        void processRequests();

        TTSRequest getNextRequest();

        bool hasHigherPriorityRequest(TTSPriority currentPriority) const;

        // TTS components
        std::unique_ptr<PiperTTS> tts_;
        std::string piperExePath_;
        std::string modelPath_;
        std::string appDirPath_;

        // Threading components
        mutable QMutex requestMutex_;
        QWaitCondition requestCondition_;
        QQueue<TTSRequest> requestQueue_;

        // State management
        std::atomic<bool> shutdownRequested_{false};
        std::atomic<bool> ttsReady_{false};
        std::atomic<uint64_t> nextRequestId_{1};

        // Current processing
        std::atomic<uint64_t> currentRequestId_{0};
        mutable QMutex currentRequestMutex_;
    };
} // namespace loki::tts
