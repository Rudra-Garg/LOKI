cmake_minimum_required(VERSION 3.21)
# Add C to the project languages so CMake knows how to compile .c files
project(loki CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# ===================================================================
# == Qt6 Configuration
# ===================================================================
# Set the CMAKE_PREFIX_PATH to help CMake find your Qt installation.
# You can set this as a CMake variable when you configure the project,
# or uncomment the line below and set it directly.


set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find the required Qt6 packages
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

# ===================================================================
# ===================================================================
# == Third-Party Library Definitions
# ===================================================================
# Define tinyexpr as its own library. This compiles tinyexpr.c into a
# static library object that we can link against later.
add_library(tinyexpr STATIC third-party/tinyexpr/tinyexpr.c)
target_include_directories(tinyexpr PUBLIC third-party/tinyexpr)


# ===================================================================
# == Main Executable Target: loki
# ===================================================================

# Define the executable using an explicit list of ALL source and header files
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

        # --- Qt UI and Worker ---
        src/gui/MainWindow.cpp
        src/core/LokiWorker.cpp
        include/loki/gui/MainWindow.h
        include/loki/core/LokiWorker.h
)


# ===================================================================
# == Dependencies and Linking for 'loki'
# ===================================================================

# --- Include Directories for 'loki' ---
# Tell our 'loki' target where to find all necessary header files
target_include_directories(loki PUBLIC
        "include"
        "third-party"
        "third-party/picovoice/include"
        "third-party/llama_cpp/include"
        "third-party/whisper_cpp/include"
)

# --- Linker Search Directories for 'loki' ---
# Tell the 'loki' target where to find external pre-compiled libraries (.lib, .a)
target_link_directories(loki PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third-party/ggml/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/third-party/llama_cpp/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper_cpp/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/third-party/picovoice/lib"
)

# --- Link Libraries for 'loki' ---
# List the libraries our 'loki' target depends on.
target_link_libraries(loki PRIVATE
        Qt6::Widgets
        # Our self-built library
        tinyexpr

        # External pre-compiled libraries
        pv_porcupine
        whisper
        llama
        ws2_32
)


# ===================================================================
# == Post-Build Commands for 'loki'
# ===================================================================

# Helper function to reduce repetition
function(COPY_RUNTIME_ASSET target_name asset_path)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${asset_path}"
            $<TARGET_FILE_DIR:${target_name}>
    )
endfunction()

# Copy all runtime assets to the build directory
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/.env")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/ggml/lib/ggml.dll")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/ggml/lib/ggml-base.dll")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/ggml/lib/ggml-cpu.dll")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/llama_cpp/lib/libllama.dll")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper_cpp/lib/libwhisper.dll")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/picovoice/lib/libpv_porcupine.dll")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/third-party/picovoice/lib/porcupine_params.pv")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/models/porcupine/Hey-Loki.ppn")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/models/whisper/ggml-base.en.bin")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/models/embedding/all-MiniLM-L6-v2.Q4_K_S.gguf")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/models/embedding/vocab.txt")
COPY_RUNTIME_ASSET(loki "${CMAKE_CURRENT_SOURCE_DIR}/data/intents.json")
# ===================================================================

# ===================================================================
# == Qt Deployment (Run windeployqt automatically)
# ===================================================================
# This command automatically finds and copies all necessary Qt DLLs
# (like Qt6Widgets.dll, plugins, etc.) to the build directory after a
# successful compilation of the 'loki' target.

# First, find the windeployqt executable in your Qt installation
find_program(WINDEPLOYQT_PATH windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin")

if(WINDEPLOYQT_PATH)
    message(STATUS "Found windeployqt: ${WINDEPLOYQT_PATH}")
    add_custom_command(
            TARGET loki POST_BUILD
            COMMAND "${WINDEPLOYQT_PATH}"
            # Use a generator expression to get the executable's directory
            # This is more robust than hard-coding a path.
            --dir "$<TARGET_FILE_DIR:loki>"
            # The executable to deploy
            "$<TARGET_FILE:loki>"
            # Optional: add --no-translations if you don't need language files
            --no-translations
            COMMENT "Running windeployqt to deploy Qt dependencies..."
    )
else()
    message(WARNING "windeployqt.exe not found. Qt DLLs will not be deployed automatically.")
endif()
# ===================================================================
