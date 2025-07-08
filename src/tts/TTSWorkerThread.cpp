#include "loki/tts/TTSWorkerThread.h"
#include <QDebug>
#include <QCoreApplication>
#include <algorithm>
#include <iostream>

namespace loki::tts {
    TTSWorkerThread::TTSWorkerThread(const std::string &piperExePath,
                                     const std::string &modelPath,
                                     const std::string &appDirPath,
                                     QObject *parent)
        : QThread(parent)
          , piperExePath_(piperExePath)
          , modelPath_(modelPath)
          , appDirPath_(appDirPath) {
        // Connect shutdown signal
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                this, &TTSWorkerThread::handleShutdown);
    }

    TTSWorkerThread::~TTSWorkerThread() {
        handleShutdown();
    }

    void TTSWorkerThread::handleShutdown() {
        if (isRunning()) {
            shutdownRequested_.store(true);

            // Wake up the worker thread
            {
                QMutexLocker locker(&requestMutex_);
                requestCondition_.wakeOne();
            }

            // Wait for thread to finish with timeout
            if (!wait(3000)) {
                std::cout << "TTS_THREAD_LOG: Force terminating TTS thread" << std::endl;
                terminate();
                wait(1000);
            }
        }
    }

    uint64_t TTSWorkerThread::synthesizeAsync(const QString &text, TTSPriority priority) {
        if (shutdownRequested_.load()) {
            return 0; // Invalid request ID
        }

        uint64_t requestId = nextRequestId_.fetch_add(1); {
            QMutexLocker locker(&requestMutex_);

            // Insert request in priority order
            TTSRequest newRequest(text, priority, requestId);

            // Find insertion point to maintain priority order
            auto insertPos = requestQueue_.begin();
            while (insertPos != requestQueue_.end() &&
                   insertPos->priority >= priority) {
                ++insertPos;
            }

            requestQueue_.insert(insertPos, newRequest);

            std::cout << "TTS_THREAD_LOG: Queued request " << requestId
                    << " with priority " << static_cast<int>(priority)
                    << ", queue size: " << requestQueue_.size() << std::endl;
        }

        // Wake up worker thread
        requestCondition_.wakeOne();

        return requestId;
    }

    void TTSWorkerThread::cancelRequest(uint64_t requestId) {
        QMutexLocker locker(&requestMutex_);

        // Mark request as cancelled in queue
        for (auto &request: requestQueue_) {
            if (request.requestId == requestId) {
                request.cancelled = true;
                std::cout << "TTS_THREAD_LOG: Cancelled request " << requestId << std::endl;
                break;
            }
        }

        // Check if we need to cancel current processing
        {
            QMutexLocker currentLocker(&currentRequestMutex_);
            if (currentRequestId_.load() == requestId) {
                // Note: For now we can't interrupt current synthesis
                // This would require more advanced Piper process management
                std::cout << "TTS_THREAD_LOG: Cannot cancel currently processing request "
                        << requestId << std::endl;
            }
        }
    }

    void TTSWorkerThread::cancelAllRequests() {
        QMutexLocker locker(&requestMutex_);

        // Mark all requests as cancelled
        for (auto &request: requestQueue_) {
            request.cancelled = true;
        }

        std::cout << "TTS_THREAD_LOG: Cancelled all " << requestQueue_.size()
                << " pending requests" << std::endl;
    }

    int TTSWorkerThread::getQueueSize() const {
        QMutexLocker locker(&requestMutex_);
        return requestQueue_.size();
    }

    void TTSWorkerThread::run() {
        std::cout << "TTS_THREAD_LOG: TTS worker thread started" << std::endl;

        // Initialize TTS
        initializeTTS();

        if (!ttsReady_.load()) {
            std::cout << "TTS_THREAD_LOG: TTS initialization failed, exiting thread" << std::endl;
            return;
        }

        // Main processing loop
        processRequests();

        std::cout << "TTS_THREAD_LOG: TTS worker thread finished" << std::endl;
    }

    void TTSWorkerThread::initializeTTS() {
        std::cout << "TTS_THREAD_LOG: Initializing TTS in worker thread..." << std::endl;

        try {
            tts_ = std::make_unique<PiperTTS>(piperExePath_, modelPath_, appDirPath_);

            if (tts_->initialize()) {
                ttsReady_.store(true);
                std::cout << "TTS_THREAD_LOG: TTS initialization successful" << std::endl;
                emit ttsInitialized(true, QString());
            } else {
                QString errorMsg = QString::fromStdString(tts_->getLastError());
                std::cout << "TTS_THREAD_LOG: TTS initialization failed: "
                        << errorMsg.toStdString() << std::endl;
                emit ttsInitialized(false, errorMsg);
            }
        } catch (const std::exception &e) {
            QString errorMsg = QString("TTS initialization exception: %1").arg(e.what());
            std::cout << "TTS_THREAD_LOG: " << errorMsg.toStdString() << std::endl;
            emit ttsInitialized(false, errorMsg);
        }
    }

    void TTSWorkerThread::processRequests() {
        std::cout << "TTS_THREAD_LOG: Starting request processing loop" << std::endl;

        while (!shutdownRequested_.load()) {
            TTSRequest request = getNextRequest();

            if (shutdownRequested_.load()) {
                break;
            }

            if (request.requestId == 0) {
                // No request available, wait
                QMutexLocker locker(&requestMutex_);
                if (!shutdownRequested_.load() && requestQueue_.isEmpty()) {
                    requestCondition_.wait(&requestMutex_, 100); // 100ms timeout
                }
                continue;
            }

            if (request.cancelled) {
                std::cout << "TTS_THREAD_LOG: Skipping cancelled request "
                        << request.requestId << std::endl;
                continue;
            }

            // Set current request
            {
                QMutexLocker currentLocker(&currentRequestMutex_);
                currentRequestId_.store(request.requestId);
            }

            std::cout << "TTS_THREAD_LOG: Processing request " << request.requestId
                    << " with text: \"" << request.text.toStdString() << "\"" << std::endl;

            // Process the request
            TTSResponse response;
            response.requestId = request.requestId;
            response.originalText = request.text;

            try {
                std::string textStd = request.text.toStdString();

                if (tts_->synthesizeToMemory(textStd, response.audioData)) {
                    response.success = true;
                    std::cout << "TTS_THREAD_LOG: Successfully synthesized "
                            << response.audioData.size() << " bytes for request "
                            << request.requestId << std::endl;
                } else {
                    response.success = false;
                    response.errorMessage = QString::fromStdString(tts_->getLastError());
                    std::cout << "TTS_THREAD_LOG: Synthesis failed for request "
                            << request.requestId << ": "
                            << response.errorMessage.toStdString() << std::endl;
                }
            } catch (const std::exception &e) {
                response.success = false;
                response.errorMessage = QString("Synthesis exception: %1").arg(e.what());
                std::cout << "TTS_THREAD_LOG: Synthesis exception for request "
                        << request.requestId << ": " << e.what() << std::endl;
            }

            // Clear current request
            {
                QMutexLocker currentLocker(&currentRequestMutex_);
                currentRequestId_.store(0);
            }

            // Emit response
            emit synthesisCompleted(response);
        }
    }

    TTSRequest TTSWorkerThread::getNextRequest() {
        QMutexLocker locker(&requestMutex_);

        if (requestQueue_.isEmpty()) {
            return TTSRequest("", TTSPriority::NORMAL, 0); // Invalid request
        }

        // Remove and return highest priority request
        TTSRequest request = requestQueue_.dequeue();

        return request;
    }

    bool TTSWorkerThread::hasHigherPriorityRequest(TTSPriority currentPriority) const {
        QMutexLocker locker(&requestMutex_);

        if (requestQueue_.isEmpty()) {
            return false;
        }

        // Check if the front request has higher priority
        return requestQueue_.front().priority > currentPriority;
    }
} // namespace loki::tts

// REMOVED: #include "TTSWorkerThread.moc" - This was causing the MOC warning
