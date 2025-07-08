#pragma once

#include <QObject>
#include <QTimer>
#include <QMap>
#include <memory>
#include "TTSWorkerThread.h"

namespace loki::tts {
    using TTSCallback = std::function<void(bool success, const std::vector<char> &audioData, const QString &error)>;

    class AsyncTTSManager : public QObject {
        Q_OBJECT

    public:
        explicit AsyncTTSManager(const std::string &piperExePath,
                                 const std::string &modelPath,
                                 const std::string &appDirPath,
                                 QObject *parent = nullptr);

        ~AsyncTTSManager() override;

        // Initialize and start the TTS system
        void initialize();

        // Shutdown the TTS system
        void shutdown();

        // Async synthesis with callback
        uint64_t synthesizeAsync(const QString &text,
                                 TTSCallback callback,
                                 TTSPriority priority = TTSPriority::NORMAL);

        // Sync synthesis (blocks until complete) - for compatibility
        bool synthesizeSync(const QString &text,
                            std::vector<char> &audioData,
                            int timeoutMs = 5000);

        // Cancel operations
        void cancelRequest(uint64_t requestId);

        void cancelAllRequests();

        // Status
        bool isReady() const;

        int getQueueSize() const;

    signals:
        void ttsReady();

        void ttsError(const QString &error);

    private slots:
        void onTTSInitialized(bool success, const QString &errorMessage);

        void onSynthesisCompleted(const TTSResponse &response);

        void onCallbackTimeout();

    private:
        void cleanupExpiredCallbacks();

        std::unique_ptr<TTSWorkerThread> workerThread_;

        // Callback management
        QMap<uint64_t, TTSCallback> pendingCallbacks_;
        QMap<uint64_t, QTimer *> callbackTimers_;
        QMutex callbackMutex_;

        // Sync operation support
        QMutex syncMutex_;
        QWaitCondition syncCondition_;

        struct SyncOperation {
            bool completed = false;
            bool success = false;
            std::vector<char> audioData;
            QString errorMessage;
        };

        QMap<uint64_t, std::shared_ptr<SyncOperation> > syncOperations_;

        bool initialized_ = false;
        static constexpr int CALLBACK_TIMEOUT_MS = 10000; // 10 second timeout
    };
} // namespace loki::tts
