
#include "loki/tts/PiperTTS.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <windows.h>
#include <vector>
#include <nlohmann/json.hpp>

namespace loki::tts {
    struct PiperTTS::Impl {
        std::string piperExePath;
        std::string modelPath;
        std::string appDirPath;
        std::string lastError;

        HANDLE hProcess = nullptr;
        HANDLE hStdinWrite = nullptr;
        HANDLE hStdoutRead = nullptr;
        bool isRunning = false;

        bool fileExists(const std::string &path) {
            return std::filesystem::exists(path);
        }

        // CORRECTED: This function reads a raw WAV stream from the pipe.
        // It reads in chunks until no more data is available from the process for this transaction.
        bool readAudioData(std::vector<char> &audioData) {
            audioData.clear();
            char buffer[4096];
            DWORD bytesRead;

            // Set the pipe to non-blocking mode to read all available data without hanging
            DWORD pipeState = PIPE_NOWAIT;
            if (!SetNamedPipeHandleState(hStdoutRead, &pipeState, NULL, NULL)) {
                // If we can't set it, we'll proceed with blocking reads, but this is less ideal.
                std::cout << "TTS_PIPE_LOG: Could not set pipe to non-blocking mode." << std::endl;
            }

            while (ReadFile(hStdoutRead, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
                audioData.insert(audioData.end(), buffer, buffer + bytesRead);
            }

            // After the first read succeeds, subsequent reads might "fail" with ERROR_NO_DATA, which is expected.
            if (audioData.empty() && GetLastError() != ERROR_NO_DATA) {
                lastError = "Failed to read from Piper output stream. Error: " + std::to_string(GetLastError());
                return false;
            }

            if (audioData.size() < 44) {
                lastError = "Received incomplete audio data from Piper. Size: " + std::to_string(audioData.size()) +
                            " bytes.";
                return false;
            }

            return true;
        }
    };

    PiperTTS::PiperTTS(const std::string &piperExePath, const std::string &modelPath, const std::string &appDirPath)
        : pImpl(std::make_unique<Impl>()) {
        pImpl->piperExePath = piperExePath;
        pImpl->modelPath = modelPath;
        pImpl->appDirPath = appDirPath;
    }

    PiperTTS::~PiperTTS() {
        if (pImpl->hStdinWrite) CloseHandle(pImpl->hStdinWrite);
        if (pImpl->isRunning && pImpl->hProcess) {
            if (WaitForSingleObject(pImpl->hProcess, 500) == WAIT_TIMEOUT) {
                TerminateProcess(pImpl->hProcess, 1);
            }
            CloseHandle(pImpl->hProcess);
        }
        if (pImpl->hStdoutRead) CloseHandle(pImpl->hStdoutRead);
    }

    bool PiperTTS::initialize() {
        std::cout << "TTS_IMPL_LOG: Starting initialize()." << std::endl;
        if (!pImpl->fileExists(pImpl->piperExePath) || !pImpl->fileExists(pImpl->modelPath)) {
            pImpl->lastError = "Piper executable or model not found.";
            std::cout << "TTS_IMPL_LOG: " << pImpl->lastError << std::endl;
            return false;
        }

        // CORRECTED: Use --output-raw AND expect JSON on stdin.
        std::stringstream cmd;
        cmd << "\"" << pImpl->piperExePath << "\"" << " --model \"" << pImpl->modelPath << "\"" <<
                " --output-raw --json-input";

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        ZeroMemory(&pi, sizeof(pi));

        HANDLE hStdinRead, hStdoutWrite;
        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        if (!CreatePipe(&hStdinRead, &pImpl->hStdinWrite, &saAttr, 0) ||
            !CreatePipe(&pImpl->hStdoutRead, &hStdoutWrite, &saAttr, 0)) {
            pImpl->lastError = "Failed to create pipes for Piper process.";
            return false;
        }

        SetHandleInformation(pImpl->hStdinWrite, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(pImpl->hStdoutRead, HANDLE_FLAG_INHERIT, 0);

        si.hStdInput = hStdinRead;
        si.hStdOutput = hStdoutWrite;
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        std::string cmdStr = cmd.str();
        std::vector<char> cmdVec(cmdStr.begin(), cmdStr.end());
        cmdVec.push_back('\0');

        if (!CreateProcessA(NULL, cmdVec.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL,
                            pImpl->appDirPath.c_str(), &si, &pi)) {
            pImpl->lastError = "CreateProcessA failed for Piper. Error: " + std::to_string(GetLastError());
            return false;
        }

        pImpl->hProcess = pi.hProcess;
        CloseHandle(pi.hThread);
        CloseHandle(hStdinRead);
        CloseHandle(hStdoutWrite);

        std::cout << "TTS_IMPL_LOG: Process launched. Proceeding to warm-up." << std::endl;

        std::vector<char> warmUpAudio;
        if (synthesizeToMemory("Ready.", warmUpAudio)) {
            pImpl->isRunning = true;
            std::cout << "TTS_IMPL_LOG: Warm-up successful." << std::endl;
            return true;
        } else {
            pImpl->lastError = "Piper warm-up synthesis failed. " + pImpl->lastError;
            std::cout << "TTS_IMPL_LOG: " << pImpl->lastError << std::endl;
            CloseHandle(pImpl->hStdinWrite);
            pImpl->hStdinWrite = nullptr;
            CloseHandle(pImpl->hStdoutRead);
            pImpl->hStdoutRead = nullptr;
            TerminateProcess(pImpl->hProcess, 1);
            CloseHandle(pImpl->hProcess);
            pImpl->hProcess = nullptr;
            return false;
        }
    }

    bool PiperTTS::synthesizeToMemory(const std::string &text, std::vector<char> &audioData) {
        if (!pImpl->isRunning || !pImpl->hStdinWrite) {
            pImpl->lastError = "Piper process is not running or pipe is invalid.";
            return false;
        }
        if (text.empty()) {
            pImpl->lastError = "Text cannot be empty";
            return false;
        }

        nlohmann::json inputJson;
        inputJson["text"] = text;
        std::string jsonString = inputJson.dump() + "\n";
        DWORD bytesWritten;

        if (!WriteFile(pImpl->hStdinWrite, jsonString.c_str(), jsonString.length(), &bytesWritten, nullptr) ||
            bytesWritten != jsonString.length()) {
            pImpl->lastError = "Failed to write to Piper process stdin. Error: " + std::to_string(GetLastError());
            return false;
        }

        // This now correctly reads the raw audio stream from stdout.
        return pImpl->readAudioData(audioData);
    }

    bool PiperTTS::synthesizeToFile(const std::string &text, const std::string &outputWavPath) {
        std::vector<char> audioData;
        if (!synthesizeToMemory(text, audioData)) {
            return false;
        }
        std::filesystem::path fullPath = pImpl->appDirPath;
        fullPath /= outputWavPath;
        std::ofstream outFile(fullPath, std::ios::binary);
        if (!outFile) {
            pImpl->lastError = "Failed to open output WAV file for writing: " + fullPath.string();
            return false;
        }
        outFile.write(audioData.data(), audioData.size());
        return true;
    }

    bool PiperTTS::isReady() const { return pImpl->isRunning; }
    std::string PiperTTS::getLastError() const { return pImpl->lastError; }
}
