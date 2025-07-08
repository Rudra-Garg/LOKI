cmake_minimum_required(VERSION 3.21)
# Add C to the project languages so CMake knows how to compile .c files
project(loki CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===================================================================
# == Qt6 Configuration
# ===================================================================
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find the required Qt6 packages
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

# ===================================================================
# == Third-Party Library Definitions
# ===================================================================
add_library(tinyexpr STATIC third-party/tinyexpr/tinyexpr.c)
target_include_directories(tinyexpr PUBLIC third-party/tinyexpr)

# ===================================================================
# == Main Executable Target: loki
# ===================================================================
add_executable(loki
        # --- Core Application Logic ---
        src/main.cpp
        src/AgentManager.cpp
        src/agents/CalculationAgent.cpp
        src/agents/SystemControlAgent.cpp
        src/core/Config.cpp
        src/core/EmbeddingModel.cpp
        src/core/OllamaClient.cpp
        src/core/Whisper.cpp
        src/intent/FastClassifier.cpp
        src/intent/IntentClassifier.cpp

        # --- TTS Integration ---
        src/tts/PiperTTS.cpp
        include/loki/tts/PiperTTS.h

        # --- Qt UI and Worker ---
        src/gui/MainWindow.cpp
        src/core/LokiWorker.cpp
        include/loki/gui/MainWindow.h
        include/loki/core/LokiWorker.h
)

if (MSVC)
    target_compile_options(loki PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/include/msvc_compat.h")
endif ()

# ===================================================================
# == Dependencies and Linking for 'loki'
# ===================================================================

# --- Include Directories for 'loki' ---
target_include_directories(loki PUBLIC
        "include"
        "third-party"
        "third-party/picovoice/include"
        "third-party/llama_cpp/include"
        "third-party/whisper_cpp/include"
        "third-party/ggml/include"
)

# --- Linker Search Directories for 'loki' ---
target_link_directories(loki PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third-party/ggml/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/third-party/llama_cpp/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper_cpp/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/third-party/picovoice/lib"
)

# --- Link Libraries for 'loki' ---
target_link_libraries(loki PRIVATE
        Qt6::Widgets
        tinyexpr # Our self-built library
        ws2_32   # Windows Socket library
)

# MSVC FIX: Use compiler-specific linking for third-party libraries.
if (MSVC)
    target_link_libraries(loki PRIVATE
            pv_porcupine.lib
            whisper.lib
            llama.lib
            winmm.lib # ADDED: For miniaudio playback functionality
    )
else ()
    target_link_libraries(loki PRIVATE
            pv_porcupine
            whisper
            llama
    )
endif ()

# ===================================================================
# == Post-Build Commands for 'loki'
# ===================================================================
# IMPORTANT: The order of POST_BUILD commands matters.
# We run windeployqt FIRST to copy Qt DLLs into a clean directory.
# Then, we copy our own third-party DLLs and assets. This avoids
# potential conflicts where windeployqt might crash if it encounters
# DLLs compiled with different runtimes (e.g., release vs. debug).

# --- Step 1: Deploy Qt Dependencies using windeployqt ---
find_program(WINDEPLOYQT_PATH windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin")

if (WINDEPLOYQT_PATH)
    message(STATUS "Found windeployqt: ${WINDEPLOYQT_PATH}")
    add_custom_command(
            TARGET loki POST_BUILD
            COMMAND "${WINDEPLOYQT_PATH}"
            --dir "$<TARGET_FILE_DIR:loki>"
            "$<TARGET_FILE:loki>"
            --no-translations
            COMMENT "Running windeployqt to deploy Qt dependencies..."
    )
else ()
    message(WARNING "windeployqt.exe not found. Qt DLLs will not be deployed automatically.")
endif ()

# --- Step 2: Copy all other runtime assets ---
function(COPY_RUNTIME_ASSET target_name asset_path)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${asset_path}"
            $<TARGET_FILE_DIR:${target_name}>
    )
endfunction()

# Copy all existing runtime assets to the build directory
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/.env")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/ggml/lib/ggml.dll")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/ggml/lib/ggml-base.dll")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/ggml/lib/ggml-cpu.dll")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/llama_cpp/lib/llama.dll")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper_cpp/lib/whisper.dll")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/picovoice/lib/pv_porcupine.dll")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/picovoice/lib/porcupine_params.pv")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/models/porcupine/Hey-Loki.ppn")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/models/whisper/ggml-base.en.bin")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/models/embedding/all-MiniLM-L6-v2.Q4_K_S.gguf")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/models/embedding/vocab.txt")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/data/intents.json")

# ===================================================================
# == Piper TTS Runtime Assets
# ===================================================================
# Copy Piper TTS executable and dependencies
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/piper/lib/piper.exe")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/piper/lib/piper_phonemize.dll")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/piper/lib/espeak-ng.dll")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/piper/lib/onnxruntime.dll")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/piper/lib/onnxruntime_providers_shared.dll")

# Copy espeak-ng-data directory
add_custom_command(TARGET loki POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/third-party/piper/share/espeak-ng-data"
        "$<TARGET_FILE_DIR:loki>/espeak-ng-data"
        COMMENT "Copying espeak-ng-data directory"
)

# Copy Piper TTS models
add_custom_command(TARGET loki POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/models/piper"
        "$<TARGET_FILE_DIR:loki>/models/piper"
        COMMENT "Copying Piper TTS models"
)