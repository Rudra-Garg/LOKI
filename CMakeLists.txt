cmake_minimum_required(VERSION 3.21)
# Add C to the project languages so CMake knows how to compile .c files
project(loki CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# Find all of our project's source files
file(GLOB_RECURSE LOKI_SOURCES "src/*.cpp")

# Define the executable using our source files
add_executable(loki ${LOKI_SOURCES})


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