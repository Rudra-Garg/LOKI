
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
        HANDLE hStderrRead = nullptr;
        bool isRunning = false;

        bool fileExists(const std::string &path) {
            return std::filesystem::exists(path);
        }

        // Check if the Piper process is still running
        bool isProcessRunning() {
            if (!hProcess) return false;
            DWORD exitCode;
            if (GetExitCodeProcess(hProcess, &exitCode)) {
                return exitCode == STILL_ACTIVE;
            }
            return false;
        }

        // Read stderr output for debugging
        std::string readStderrOutput(int timeoutMs = 1000) {
            if (!hStderrRead) return "";
            
            std::string stderrOutput;
            char buffer[1024];
            DWORD bytesRead;
            DWORD startTime = GetTickCount();
            
            // Set pipe to non-blocking mode
            DWORD pipeState = PIPE_NOWAIT;
            SetNamedPipeHandleState(hStderrRead, &pipeState, NULL, NULL);
            
            while ((GetTickCount() - startTime) < timeoutMs) {
                if (ReadFile(hStderrRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    stderrOutput += buffer;
                } else {
                    DWORD error = GetLastError();
                    if (error != ERROR_NO_DATA) break;
                    Sleep(10); // Brief pause before retry
                }
            }
            
            return stderrOutput;
        }

        // IMPROVED: This function reads a raw WAV stream from the pipe with better error handling
        // It reads in chunks until no more data is available, with timeout and process health checks
        bool readAudioData(std::vector<char> &audioData) {
            audioData.clear();
            
            // Validate stdout pipe before attempting to read
            if (!hStdoutRead || hStdoutRead == INVALID_HANDLE_VALUE) {
                lastError = "Stdout pipe is invalid";
                std::cout << "TTS_PIPE_LOG: " << lastError << std::endl;
                return false;
            }
            
            char buffer[4096];
            DWORD bytesRead;
            DWORD startTime = GetTickCount();
            const DWORD timeoutMs = 10000; // 10 second timeout
            bool hasReceivedData = false;

            std::cout << "TTS_PIPE_LOG: Starting to read audio data from Piper..." << std::endl;

            // Set the pipe to non-blocking mode to read all available data without hanging
            DWORD pipeState = PIPE_NOWAIT;
            if (!SetNamedPipeHandleState(hStdoutRead, &pipeState, NULL, NULL)) {
                DWORD error = GetLastError();
                std::cout << "TTS_PIPE_LOG: Warning - Could not set pipe to non-blocking mode. Error: " << error << std::endl;
                // Continue anyway as this might fail for anonymous pipes
            }

            // First, wait a moment for Piper to process and start outputting
            Sleep(100);

            while ((GetTickCount() - startTime) < timeoutMs) {
                // Check if process is still running
                if (!isProcessRunning()) {
                    lastError = "Piper process has terminated unexpectedly";
                    std::cout << "TTS_PIPE_LOG: " << lastError << std::endl;
                    
                    // Try to read stderr for error information
                    std::string stderrOutput = readStderrOutput(500);
                    if (!stderrOutput.empty()) {
                        std::cout << "TTS_PIPE_LOG: Piper stderr: " << stderrOutput << std::endl;
                        lastError += ". Stderr: " + stderrOutput;
                    }
                    return false;
                }

                if (ReadFile(hStdoutRead, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
                    // Log first few bytes to see what we're getting
                    if (!hasReceivedData) {
                        std::cout << "TTS_PIPE_LOG: First " << std::min((DWORD)32, bytesRead) << " bytes received: ";
                        for (DWORD i = 0; i < std::min((DWORD)32, bytesRead); ++i) {
                            if (buffer[i] >= 32 && buffer[i] <= 126) {
                                std::cout << buffer[i];
                            } else {
                                std::cout << "\\x" << std::hex << (unsigned char)buffer[i] << std::dec;
                            }
                        }
                        std::cout << std::endl;
                        hasReceivedData = true;
                    }

                    audioData.insert(audioData.end(), buffer, buffer + bytesRead);
                    std::cout << "TTS_PIPE_LOG: Read " << bytesRead << " bytes, total: " << audioData.size() << std::endl;
                    
                    // Reset timeout when we receive data
                    startTime = GetTickCount();
                } else {
                    DWORD error = GetLastError();
                    if (error != ERROR_NO_DATA && error != ERROR_MORE_DATA) {
                        lastError = "Failed to read from Piper output stream. Error: " + std::to_string(error);
                        std::cout << "TTS_PIPE_LOG: " << lastError << std::endl;
                        return false;
                    }
                    
                    // If we have data and no more is immediately available, check if it's sufficient
                    if (hasReceivedData && audioData.size() >= 44) {
                        // Brief pause to see if more data is coming
                        Sleep(50);
                        
                        // Try one more read
                        if (ReadFile(hStdoutRead, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
                            audioData.insert(audioData.end(), buffer, buffer + bytesRead);
                            std::cout << "TTS_PIPE_LOG: Read additional " << bytesRead << " bytes, total: " << audioData.size() << std::endl;
                        } else {
                            // No more data available, proceed with what we have
                            break;
                        }
                    }
                    
                    Sleep(10); // Brief pause before retry
                }
            }

            // Check if we timed out
            if ((GetTickCount() - startTime) >= timeoutMs) {
                lastError = "Timeout reading from Piper output stream after " + std::to_string(timeoutMs) + "ms";
                std::cout << "TTS_PIPE_LOG: " << lastError << std::endl;
                
                // Try to read stderr for additional info
                std::string stderrOutput = readStderrOutput(500);
                if (!stderrOutput.empty()) {
                    std::cout << "TTS_PIPE_LOG: Piper stderr during timeout: " << stderrOutput << std::endl;
                }
                return false;
            }

            std::cout << "TTS_PIPE_LOG: Finished reading. Total audio data size: " << audioData.size() << " bytes" << std::endl;

            // Check if we received data but it's too small
            if (audioData.empty()) {
                lastError = "No audio data received from Piper";
                std::cout << "TTS_PIPE_LOG: " << lastError << std::endl;
                
                // Try to read stderr for additional info
                std::string stderrOutput = readStderrOutput(500);
                if (!stderrOutput.empty()) {
                    std::cout << "TTS_PIPE_LOG: Piper stderr when no data: " << stderrOutput << std::endl;
                    lastError += ". Stderr: " + stderrOutput;
                }
                return false;
            }

            if (audioData.size() < 44) {
                lastError = "Received incomplete audio data from Piper. Size: " + std::to_string(audioData.size()) +
                            " bytes (expected at least 44 for WAV header)";
                std::cout << "TTS_PIPE_LOG: " << lastError << std::endl;
                
                // Show what we actually received
                std::cout << "TTS_PIPE_LOG: Received data: ";
                for (size_t i = 0; i < std::min(audioData.size(), (size_t)64); ++i) {
                    if (audioData[i] >= 32 && audioData[i] <= 126) {
                        std::cout << audioData[i];
                    } else {
                        std::cout << "\\x" << std::hex << (unsigned char)audioData[i] << std::dec;
                    }
                }
                std::cout << std::endl;
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
        if (pImpl->hStderrRead) CloseHandle(pImpl->hStderrRead);
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

        HANDLE hStdinRead, hStdoutWrite, hStderrWrite;
        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        // Create pipes individually with detailed error checking
        if (!CreatePipe(&hStdinRead, &pImpl->hStdinWrite, &saAttr, 0)) {
            DWORD error = GetLastError();
            pImpl->lastError = "Failed to create stdin pipe for Piper process. Error: " + std::to_string(error);
            std::cout << "TTS_IMPL_LOG: " << pImpl->lastError << std::endl;
            return false;
        }
        
        if (!CreatePipe(&pImpl->hStdoutRead, &hStdoutWrite, &saAttr, 0)) {
            DWORD error = GetLastError();
            pImpl->lastError = "Failed to create stdout pipe for Piper process. Error: " + std::to_string(error);
            std::cout << "TTS_IMPL_LOG: " << pImpl->lastError << std::endl;
            // Clean up stdin pipe
            CloseHandle(hStdinRead);
            CloseHandle(pImpl->hStdinWrite);
            pImpl->hStdinWrite = nullptr;
            return false;
        }
        
        if (!CreatePipe(&pImpl->hStderrRead, &hStderrWrite, &saAttr, 0)) {
            DWORD error = GetLastError();
            pImpl->lastError = "Failed to create stderr pipe for Piper process. Error: " + std::to_string(error);
            std::cout << "TTS_IMPL_LOG: " << pImpl->lastError << std::endl;
            // Clean up stdin and stdout pipes
            CloseHandle(hStdinRead);
            CloseHandle(pImpl->hStdinWrite);
            CloseHandle(pImpl->hStdoutRead);
            CloseHandle(hStdoutWrite);
            pImpl->hStdinWrite = nullptr;
            pImpl->hStdoutRead = nullptr;
            return false;
        }

        // Validate pipe handles
        if (pImpl->hStdinWrite == INVALID_HANDLE_VALUE || pImpl->hStdoutRead == INVALID_HANDLE_VALUE || 
            pImpl->hStderrRead == INVALID_HANDLE_VALUE) {
            pImpl->lastError = "One or more pipe handles are invalid after creation";
            std::cout << "TTS_IMPL_LOG: " << pImpl->lastError << std::endl;
            // Clean up all pipes
            CloseHandle(hStdinRead);
            CloseHandle(pImpl->hStdinWrite);
            CloseHandle(pImpl->hStdoutRead);
            CloseHandle(hStdoutWrite);
            CloseHandle(pImpl->hStderrRead);
            CloseHandle(hStderrWrite);
            pImpl->hStdinWrite = nullptr;
            pImpl->hStdoutRead = nullptr;
            pImpl->hStderrRead = nullptr;
            return false;
        }

        // Configure handle inheritance - prevent child handles from being inherited by grandchildren
        if (!SetHandleInformation(pImpl->hStdinWrite, HANDLE_FLAG_INHERIT, 0)) {
            DWORD error = GetLastError();
            std::cout << "TTS_IMPL_LOG: Warning - Failed to set stdin write handle inheritance. Error: " << error << std::endl;
        }
        if (!SetHandleInformation(pImpl->hStdoutRead, HANDLE_FLAG_INHERIT, 0)) {
            DWORD error = GetLastError();
            std::cout << "TTS_IMPL_LOG: Warning - Failed to set stdout read handle inheritance. Error: " << error << std::endl;
        }
        if (!SetHandleInformation(pImpl->hStderrRead, HANDLE_FLAG_INHERIT, 0)) {
            DWORD error = GetLastError();
            std::cout << "TTS_IMPL_LOG: Warning - Failed to set stderr read handle inheritance. Error: " << error << std::endl;
        }

        si.hStdInput = hStdinRead;
        si.hStdOutput = hStdoutWrite;
        si.hStdError = hStderrWrite;

        std::string cmdStr = cmd.str();
        std::vector<char> cmdVec(cmdStr.begin(), cmdStr.end());
        cmdVec.push_back('\0');

        if (!CreateProcessA(NULL, cmdVec.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL,
                            pImpl->appDirPath.c_str(), &si, &pi)) {
            DWORD error = GetLastError();
            pImpl->lastError = "CreateProcessA failed for Piper. Error: " + std::to_string(error);
            std::cout << "TTS_IMPL_LOG: " << pImpl->lastError << std::endl;
            
            // Clean up all pipe handles
            CloseHandle(hStdinRead);
            CloseHandle(hStdoutWrite);
            CloseHandle(hStderrWrite);
            CloseHandle(pImpl->hStdinWrite);
            CloseHandle(pImpl->hStdoutRead);
            CloseHandle(pImpl->hStderrRead);
            pImpl->hStdinWrite = nullptr;
            pImpl->hStdoutRead = nullptr;
            pImpl->hStderrRead = nullptr;
            return false;
        }

        pImpl->hProcess = pi.hProcess;
        CloseHandle(pi.hThread);
        CloseHandle(hStdinRead);
        CloseHandle(hStdoutWrite);
        CloseHandle(hStderrWrite);

        std::cout << "TTS_IMPL_LOG: Process launched successfully. PID: " << pi.dwProcessId << std::endl;
        
        // Give Piper a moment to initialize
        Sleep(500);
        
        // Check if process is still running after initialization
        if (!pImpl->isProcessRunning()) {
            pImpl->lastError = "Piper process terminated immediately after launch";
            std::cout << "TTS_IMPL_LOG: " << pImpl->lastError << std::endl;
            
            // Try to read stderr for error information
            std::string stderrOutput = pImpl->readStderrOutput(1000);
            if (!stderrOutput.empty()) {
                std::cout << "TTS_IMPL_LOG: Piper stderr: " << stderrOutput << std::endl;
                pImpl->lastError += ". Stderr: " + stderrOutput;
            }
            return false;
        }

        // Additional validation: verify pipes are still valid and writable
        std::cout << "TTS_IMPL_LOG: Validating pipe handles before warm-up..." << std::endl;
        
        // Test that we can write to stdin pipe
        DWORD bytesWritten;
        const char testData[] = "";  // Empty test write
        if (!WriteFile(pImpl->hStdinWrite, testData, 0, &bytesWritten, nullptr)) {
            DWORD error = GetLastError();
            pImpl->lastError = "Stdin pipe validation failed. Error: " + std::to_string(error);
            std::cout << "TTS_IMPL_LOG: " << pImpl->lastError << std::endl;
            return false;
        }
        
        // Verify stdout pipe is readable by checking its state
        DWORD pipeState, curInstances, maxCollectionCount, collectDataTimeout;
        if (!GetNamedPipeHandleState(pImpl->hStdoutRead, &pipeState, &curInstances, 
                                     &maxCollectionCount, &collectDataTimeout, nullptr, 0)) {
            // This might fail for anonymous pipes, so only log as warning
            DWORD error = GetLastError();
            std::cout << "TTS_IMPL_LOG: Warning - Could not get stdout pipe state. Error: " << error << std::endl;
        }

        std::cout << "TTS_IMPL_LOG: Process running and pipes validated. Proceeding to warm-up." << std::endl;

        std::vector<char> warmUpAudio;
        if (synthesizeToMemory("Ready.", warmUpAudio)) {
            pImpl->isRunning = true;
            std::cout << "TTS_IMPL_LOG: Warm-up successful. Audio size: " << warmUpAudio.size() << " bytes" << std::endl;
            return true;
        } else {
            pImpl->lastError = "Piper warm-up synthesis failed. " + pImpl->lastError;
            std::cout << "TTS_IMPL_LOG: " << pImpl->lastError << std::endl;
            
            // Try to get additional diagnostic information
            std::string stderrOutput = pImpl->readStderrOutput(1000);
            if (!stderrOutput.empty()) {
                std::cout << "TTS_IMPL_LOG: Additional Piper stderr: " << stderrOutput << std::endl;
            }
            
            // Clean up resources
            CloseHandle(pImpl->hStdinWrite);
            pImpl->hStdinWrite = nullptr;
            CloseHandle(pImpl->hStdoutRead);
            pImpl->hStdoutRead = nullptr;
            if (pImpl->hStderrRead) {
                CloseHandle(pImpl->hStderrRead);
                pImpl->hStderrRead = nullptr;
            }
            TerminateProcess(pImpl->hProcess, 1);
            CloseHandle(pImpl->hProcess);
            pImpl->hProcess = nullptr;
            return false;
        }
    }

    bool PiperTTS::synthesizeToMemory(const std::string &text, std::vector<char> &audioData) {
        if (!pImpl->isRunning) {
            pImpl->lastError = "Piper process is not running.";
            return false;
        }
        
        // Validate pipe handles
        if (!pImpl->hStdinWrite || pImpl->hStdinWrite == INVALID_HANDLE_VALUE) {
            pImpl->lastError = "Stdin pipe is invalid.";
            return false;
        }
        
        if (!pImpl->hStdoutRead || pImpl->hStdoutRead == INVALID_HANDLE_VALUE) {
            pImpl->lastError = "Stdout pipe is invalid.";
            return false;
        }
        
        // Additional process health check
        if (!pImpl->isProcessRunning()) {
            pImpl->lastError = "Piper process has terminated unexpectedly";
            pImpl->isRunning = false;
            return false;
        }
        
        if (text.empty()) {
            pImpl->lastError = "Text cannot be empty";
            return false;
        }

        std::cout << "TTS_SYNTHESIS_LOG: Synthesizing text: \"" << text << "\"" << std::endl;

        nlohmann::json inputJson;
        inputJson["text"] = text;
        std::string jsonString = inputJson.dump() + "\n";
        DWORD bytesWritten;

        std::cout << "TTS_SYNTHESIS_LOG: Sending JSON: " << jsonString << std::endl;

        if (!WriteFile(pImpl->hStdinWrite, jsonString.c_str(), jsonString.length(), &bytesWritten, nullptr) ||
            bytesWritten != jsonString.length()) {
            DWORD error = GetLastError();
            pImpl->lastError = "Failed to write to Piper process stdin. Error: " + std::to_string(error);
            std::cout << "TTS_SYNTHESIS_LOG: " << pImpl->lastError << std::endl;
            return false;
        }

        std::cout << "TTS_SYNTHESIS_LOG: Successfully wrote " << bytesWritten << " bytes to stdin" << std::endl;

        // This now correctly reads the raw audio stream from stdout with improved error handling
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
