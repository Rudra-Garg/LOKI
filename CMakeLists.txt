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
# == Testing Configuration (Google Test)
# ===================================================================
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent gtest from using /Zc:__cplusplus
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
# ===================================================================
# ===================================================================
# == Third-Party Library Definitions
# ===================================================================
add_library(tinyexpr STATIC third-party/tinyexpr/tinyexpr.c)
target_include_directories(tinyexpr PUBLIC third-party/tinyexpr)

# ===================================================================
# == Source Files Definition
# ===================================================================
# NOTE: src/main.cpp is EXCLUDED from this list as it belongs only to the executable.
set(CORE_SOURCES
        # --- Core Application Logic ---
        src/AgentManager.cpp
        src/agents/windows/WindowsSystemControl.cpp
        src/agents/CalculationAgent.cpp
        src/agents/SystemControlAgent.cpp
        src/core/Config.cpp
        src/core/EmbeddingModel.cpp
        src/core/OllamaClient.cpp
        src/core/Whisper.cpp
        src/intent/FastClassifier.cpp
        src/intent/IntentClassifier.cpp

        # --- Qt UI and Worker ---
        src/core/LokiWorker.cpp
)

set(TTS_SOURCES
        # --- TTS Integration ---
        src/tts/PiperTTS.cpp
        src/tts/TTSWorkerThread.cpp
        src/tts/AsyncTTSManager.cpp
)

set(HEADER_FILES
        # --- Core Headers ---
        include/loki/core/LokiWorker.h

        # --- TTS Headers ---
        include/loki/tts/PiperTTS.h
        include/loki/tts/TTSWorkerThread.h
        include/loki/tts/AsyncTTSManager.h
)

# ===================================================================
# == Core Logic Library: loki_lib
# ===================================================================
add_library(loki_lib
        ${CORE_SOURCES}
        ${TTS_SOURCES}
        ${HEADER_FILES}
)

target_include_directories(loki_lib PUBLIC
        "include"
        "third-party"
        "third-party/picovoice/include"
        "third-party/llama_cpp/include"
        "third-party/whisper_cpp/include"
        "third-party/ggml/include"
)

target_link_directories(loki_lib PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/third-party/ggml/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/third-party/llama_cpp/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper_cpp/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/third-party/picovoice/lib"
)

target_link_libraries(loki_lib PUBLIC
        Qt6::Widgets
        Qt6::Core
        Qt6::Gui
        tinyexpr
        ws2_32
        pv_porcupine.lib
        whisper.lib
        llama.lib
        winmm.lib
        ole32.lib
        PowrProf.lib
)

if (MSVC)
    target_compile_options(loki_lib PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/include/msvc_compat.h")
endif ()

# ===================================================================
# == Main Executable Target: loki
# ===================================================================
qt_add_executable(loki
        src/main.cpp
        src/gui/MainWindow.cpp

        include/loki/gui/MainWindow.h
        )
target_link_libraries(loki PRIVATE loki_lib)

# ===================================================================
# == Asset Deployment Function
# ===================================================================
# This function wraps all the asset copying logic so it can be reused
# for both the main executable and the test executable.
function(DEPLOY_LOKI_TARGET target_name)
    # --- Step 1: Deploy Qt Dependencies using windeployqt ---
    find_program(WINDEPLOYQT_PATH windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin")
    if (WINDEPLOYQT_PATH)
        add_custom_command(
                TARGET ${target_name} POST_BUILD
                COMMAND "${WINDEPLOYQT_PATH}" --dir "$<TARGET_FILE_DIR:${target_name}>" "$<TARGET_FILE:${target_name}>" --no-translations
                COMMENT "Running windeployqt for ${target_name}..."
        )
    endif ()

    # --- Step 2: Copy all other runtime assets ---
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/.env" $<TARGET_FILE_DIR:${target_name}>)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third-party/ggml/lib/ggml.dll" $<TARGET_FILE_DIR:${target_name}>)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third-party/ggml/lib/ggml-base.dll" $<TARGET_FILE_DIR:${target_name}>)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third-party/ggml/lib/ggml-cpu.dll" $<TARGET_FILE_DIR:${target_name}>)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third-party/llama_cpp/lib/llama.dll" $<TARGET_FILE_DIR:${target_name}>)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper_cpp/lib/whisper.dll" $<TARGET_FILE_DIR:${target_name}>)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third-party/picovoice/lib/pv_porcupine.dll" $<TARGET_FILE_DIR:${target_name}>)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third-party/picovoice/lib/porcupine_params.pv" $<TARGET_FILE_DIR:${target_name}>)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/models/porcupine/Hey-Loki.ppn" $<TARGET_FILE_DIR:${target_name}>)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/models/whisper/ggml-base.en.bin" $<TARGET_FILE_DIR:${target_name}>)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/models/embedding/all-MiniLM-L6-v2.Q4_K_S.gguf" $<TARGET_FILE_DIR:${target_name}>)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/models/embedding/vocab.txt" $<TARGET_FILE_DIR:${target_name}>)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/data/intents.json" $<TARGET_FILE_DIR:${target_name}>)

    # --- Step 3: Copy Piper TTS Assets ---
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third-party/piper/lib/piper.exe" $<TARGET_FILE_DIR:${target_name}>)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third-party/piper/lib/piper_phonemize.dll" $<TARGET_FILE_DIR:${target_name}>)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third-party/piper/lib/espeak-ng.dll" $<TARGET_FILE_DIR:${target_name}>)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third-party/piper/lib/onnxruntime.dll" $<TARGET_FILE_DIR:${target_name}>)
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third-party/piper/lib/onnxruntime_providers_shared.dll" $<TARGET_FILE_DIR:${target_name}>)

    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/third-party/piper/share/espeak-ng-data" "$<TARGET_FILE_DIR:${target_name}>/espeak-ng-data")
    add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/models/piper" "$<TARGET_FILE_DIR:${target_name}>/models/piper")
endfunction()

# Deploy assets for the main application
DEPLOY_LOKI_TARGET(loki)

# ===================================================================
# == Test Executable Target: loki_tests
# ===================================================================
if (BUILD_TESTING)
    enable_testing()

    add_executable(loki_tests
            tests/main.cpp
            tests/test_calculation_agent.cpp
            tests/test_system_control_agent.cpp
            tests/test_system_control_integration.cpp
    )

    target_link_libraries(loki_tests PRIVATE loki_lib gtest_main)

    target_include_directories(loki_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests" # Allow #include "mocks/MockSystemControl.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/third-party"
    )

    # Deploy assets for the test executable as well
    DEPLOY_LOKI_TARGET(loki_tests)

    include(GoogleTest)
    gtest_discover_tests(loki_tests)
endif ()