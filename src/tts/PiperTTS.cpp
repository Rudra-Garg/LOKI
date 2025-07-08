#include "loki/tts/PiperTTS.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <windows.h>
#include <vector>
#include <nlohmann/json.hpp>

namespace loki {
    namespace tts {
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

            // MODIFIED: This function is now robust against logging messages from Piper.
            bool readAudioData(std::vector<char> &audioData) {
                audioData.clear();
                std::string jsonLine;
                bool jsonFound = false;

                // Loop to find the JSON header, skipping any non-JSON logging lines from Piper.
                // We'll try up to 10 lines before giving up.
                for (int i = 0; i < 10; ++i) {
                    jsonLine.clear();
                    char ch;
                    DWORD bytesRead;

                    // Read one line from the pipe
                    while (ReadFile(hStdoutRead, &ch, 1, &bytesRead, nullptr) && bytesRead > 0) {
                        if (ch == '\n') break;
                        jsonLine += ch;
                    }

                    if (jsonLine.empty()) continue;

                    // Check if the line looks like a JSON object.
                    size_t first = jsonLine.find_first_not_of(" \t\r\n");
                    if (first != std::string::npos && jsonLine[first] == '{') {
                        jsonFound = true;
                        break; // Found it!
                    } else {
                        // This is a log line from Piper, not data. Print it for diagnostics and ignore it.
                        std::cout << "TTS_PIPE_LOG (Skipped): " << jsonLine << std::endl;
                    }
                }

                if (!jsonFound) {
                    lastError = "Failed to find JSON header in Piper's output stream.";
                    return false;
                }

                long audioBytes = 0;
                try {
                    auto j = nlohmann::json::parse(jsonLine);
                    audioBytes = j.value("audio_bytes", 0);
                } catch (const std::exception &e) {
                    lastError = "Failed to parse JSON header from Piper: " + std::string(e.what()) + ". Raw line: " +
                                jsonLine;
                    return false;
                }

                if (audioBytes <= 0) {
                    lastError = "Piper reported 0 or invalid audio bytes in JSON header.";
                    return false;
                }

                // Now read exactly that many bytes for the WAV data
                audioData.resize(audioBytes);
                DWORD totalBytesRead = 0;
                DWORD bytesRead;
                while (totalBytesRead < audioBytes) {
                    DWORD bytesToRead = audioBytes - totalBytesRead;
                    if (ReadFile(hStdoutRead, audioData.data() + totalBytesRead, bytesToRead, &bytesRead, nullptr) &&
                        bytesRead > 0) {
                        totalBytesRead += bytesRead;
                    } else {
                        lastError = "Failed to read full audio chunk from Piper pipe. Error: " + std::to_string(
                                        GetLastError());
                        return false;
                    }
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

            std::stringstream cmd;
            cmd << "\"" << pImpl->piperExePath << "\"" << " --model \"" << pImpl->modelPath << "\"" << " --json-input";

            STARTUPINFOA si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES;
            ZeroMemory(&pi, sizeof(pi));

            HANDLE hStdinRead, hStdoutWrite, hStderrRead, hStderrWrite;
            SECURITY_ATTRIBUTES saAttr;
            saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
            saAttr.bInheritHandle = TRUE;
            saAttr.lpSecurityDescriptor = NULL;

            if (!CreatePipe(&hStdinRead, &pImpl->hStdinWrite, &saAttr, 0) ||
                !CreatePipe(&pImpl->hStdoutRead, &hStdoutWrite, &saAttr, 0) ||
                !CreatePipe(&hStderrRead, &hStderrWrite, &saAttr, 0)) {
                // Pipe for stderr
                pImpl->lastError = "Failed to create pipes for Piper process.";
                return false;
            }

            SetHandleInformation(pImpl->hStdinWrite, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(pImpl->hStdoutRead, HANDLE_FLAG_INHERIT, 0);

            si.hStdInput = hStdinRead;
            si.hStdOutput = hStdoutWrite;
            si.hStdError = hStderrWrite; // Redirect stderr to its own pipe

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
            CloseHandle(hStderrWrite); // Close parent's write handle to stderr pipe

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
            if (!pImpl->hStdinWrite) {
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
                pImpl->lastError = "Failed to write to Piper process stdin.";
                return false;
            }
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
    } // namespace tts
} // namespace loki
