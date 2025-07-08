#include "loki/tts/AsyncTTSManager.h"
#include <QDebug>
#include <QMutexLocker>
#include <QVariant>
#include <iostream>

namespace loki::tts {
    AsyncTTSManager::AsyncTTSManager(const std::string &piperExePath,
                                     const std::string &modelPath,
                                     const std::string &appDirPath,
                                     QObject *parent)
        : QObject(parent) {
        workerThread_ = std::make_unique<TTSWorkerThread>(piperExePath, modelPath, appDirPath);

        // Connect signals
        connect(workerThread_.get(), &TTSWorkerThread::ttsInitialized,
                this, &AsyncTTSManager::onTTSInitialized);
        connect(workerThread_.get(), &TTSWorkerThread::synthesisCompleted,
                this, &AsyncTTSManager::onSynthesisCompleted);
        connect(workerThread_.get(), &TTSWorkerThread::ttsError,
                this, &AsyncTTSManager::ttsError);
    }

    AsyncTTSManager::~AsyncTTSManager() {
        shutdown();
    }

    void AsyncTTSManager::initialize() {
        if (initialized_) {
            return;
        }

        std::cout << "ASYNC_TTS_LOG: Initializing async TTS manager..." << std::endl;
        workerThread_->start();
        initialized_ = true;
    }

    void AsyncTTSManager::shutdown() {
        if (!initialized_) {
            return;
        }

        std::cout << "ASYNC_TTS_LOG: Shutting down async TTS manager..." << std::endl;

        // Cancel all pending operations
        cancelAllRequests();

        // Stop worker thread
        if (workerThread_) {
            workerThread_.reset();
        }

        // Clean up timers
        {
            QMutexLocker locker(&callbackMutex_);
            for (auto timer: callbackTimers_) {
                timer->stop();
                timer->deleteLater();
            }
            callbackTimers_.clear();
            pendingCallbacks_.clear();
        }

        initialized_ = false;
    }

    uint64_t AsyncTTSManager::synthesizeAsync(const QString &text,
                                              TTSCallback callback,
                                              TTSPriority priority) {
        if (!initialized_ || !workerThread_) {
            std::cout << "ASYNC_TTS_LOG: TTS not initialized, cannot process request" << std::endl;
            if (callback) {
                callback(false, {}, "TTS not initialized");
            }
            return 0;
        }

        uint64_t requestId = workerThread_->synthesizeAsync(text, priority);
        if (requestId == 0) {
            std::cout << "ASYNC_TTS_LOG: Failed to queue TTS request" << std::endl;
            if (callback) {
                callback(false, {}, "Failed to queue request");
            }
            return 0;
        }

        if (callback) {
            QMutexLocker locker(&callbackMutex_);

            // Store callback
            pendingCallbacks_[requestId] = callback;

            // Set up timeout timer
            QTimer *timer = new QTimer(this);
            timer->setSingleShot(true);
            timer->setProperty("requestId", QVariant::fromValue(requestId));
            connect(timer, &QTimer::timeout, this, &AsyncTTSManager::onCallbackTimeout);
            timer->start(CALLBACK_TIMEOUT_MS);
            callbackTimers_[requestId] = timer;

            std::cout << "ASYNC_TTS_LOG: Registered callback for request " << requestId << std::endl;
        }

        return requestId;
    }

    bool AsyncTTSManager::synthesizeSync(const QString &text,
                                         std::vector<char> &audioData,
                                         int timeoutMs) {
        if (!initialized_ || !workerThread_) {
            std::cout << "ASYNC_TTS_LOG: TTS not initialized for sync request" << std::endl;
            return false;
        }

        auto syncOp = std::make_shared<SyncOperation>();

        uint64_t requestId = workerThread_->synthesizeAsync(text, TTSPriority::HIGH);
        if (requestId == 0) {
            std::cout << "ASYNC_TTS_LOG: Failed to queue sync TTS request" << std::endl;
            return false;
        } {
            QMutexLocker locker(&syncMutex_);
            syncOperations_[requestId] = syncOp;
        }

        // Wait for completion
        {
            QMutexLocker locker(&syncMutex_);
            if (!syncOp->completed) {
                syncCondition_.wait(&syncMutex_, timeoutMs);
            }
        }

        // Check result
        bool success = syncOp->completed && syncOp->success;
        if (success) {
            audioData = std::move(syncOp->audioData);
            std::cout << "ASYNC_TTS_LOG: Sync synthesis completed successfully for request "
                    << requestId << std::endl;
        } else {
            std::cout << "ASYNC_TTS_LOG: Sync synthesis failed for request " << requestId;
            if (!syncOp->errorMessage.isEmpty()) {
                std::cout << ": " << syncOp->errorMessage.toStdString();
            }
            std::cout << std::endl;
        }

        // Cleanup
        {
            QMutexLocker locker(&syncMutex_);
            syncOperations_.remove(requestId);
        }

        return success;
    }

    void AsyncTTSManager::cancelRequest(uint64_t requestId) {
        if (workerThread_) {
            workerThread_->cancelRequest(requestId);
        }

        // Clean up callback
        {
            QMutexLocker locker(&callbackMutex_);
            if (callbackTimers_.contains(requestId)) {
                callbackTimers_[requestId]->stop();
                callbackTimers_[requestId]->deleteLater();
                callbackTimers_.remove(requestId);
            }
            pendingCallbacks_.remove(requestId);
        }

        std::cout << "ASYNC_TTS_LOG: Cancelled request " << requestId << std::endl;
    }

    void AsyncTTSManager::cancelAllRequests() {
        if (workerThread_) {
            workerThread_->cancelAllRequests();
        }

        // Clean up all callbacks
        {
            QMutexLocker locker(&callbackMutex_);
            for (auto timer: callbackTimers_) {
                timer->stop();
                timer->deleteLater();
            }
            callbackTimers_.clear();
            pendingCallbacks_.clear();
        }

        std::cout << "ASYNC_TTS_LOG: Cancelled all requests" << std::endl;
    }

    bool AsyncTTSManager::isReady() const {
        return workerThread_ && workerThread_->isReady();
    }

    int AsyncTTSManager::getQueueSize() const {
        return workerThread_ ? workerThread_->getQueueSize() : 0;
    }

    void AsyncTTSManager::onTTSInitialized(bool success, const QString &errorMessage) {
        if (success) {
            std::cout << "ASYNC_TTS_LOG: TTS initialization completed successfully" << std::endl;
            emit ttsReady();
        } else {
            std::cout << "ASYNC_TTS_LOG: TTS initialization failed: "
                    << errorMessage.toStdString() << std::endl;
            emit ttsError(errorMessage);
        }
    }

    void AsyncTTSManager::onSynthesisCompleted(const TTSResponse &response) {
        std::cout << "ASYNC_TTS_LOG: Synthesis completed for request " << response.requestId
                << " (success: " << response.success << ")" << std::endl;

        // Handle async callback
        {
            QMutexLocker locker(&callbackMutex_);
            if (pendingCallbacks_.contains(response.requestId)) {
                TTSCallback callback = pendingCallbacks_[response.requestId];
                pendingCallbacks_.remove(response.requestId);

                // Clean up timer
                if (callbackTimers_.contains(response.requestId)) {
                    callbackTimers_[response.requestId]->stop();
                    callbackTimers_[response.requestId]->deleteLater();
                    callbackTimers_.remove(response.requestId);
                }

                // Call callback outside of lock
                locker.unlock();
                callback(response.success, response.audioData, response.errorMessage);
            }
        }

        // Handle sync operation
        {
            QMutexLocker locker(&syncMutex_);
            if (syncOperations_.contains(response.requestId)) {
                auto syncOp = syncOperations_[response.requestId];
                syncOp->completed = true;
                syncOp->success = response.success;
                syncOp->audioData = response.audioData;
                syncOp->errorMessage = response.errorMessage;
                syncCondition_.wakeAll();
            }
        }
    }

    void AsyncTTSManager::onCallbackTimeout() {
        QTimer *timer = qobject_cast<QTimer *>(sender());
        if (!timer) return;

        uint64_t requestId = timer->property("requestId").value<uint64_t>();

        std::cout << "ASYNC_TTS_LOG: Callback timeout for request " << requestId << std::endl; {
            QMutexLocker locker(&callbackMutex_);
            if (pendingCallbacks_.contains(requestId)) {
                TTSCallback callback = pendingCallbacks_[requestId];
                pendingCallbacks_.remove(requestId);
                callbackTimers_.remove(requestId);

                // Call callback with timeout error
                locker.unlock();
                callback(false, {}, "Request timeout");
            }
        }

        timer->deleteLater();
    }
} // namespace loki::tts

// REMOVED: #include "AsyncTTSManager.moc" - This was causing the MOC warning
