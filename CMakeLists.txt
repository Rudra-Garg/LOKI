cmake_minimum_required(VERSION 3.15)

project(loki VERSION 1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Define Library Paths ---
# These variables make the rest of the script cleaner.

# Porcupine (for wake word detection)
set(PORCUPINE_LIB ${CMAKE_CURRENT_SOURCE_DIR}/lib/windows/amd64/libpv_porcupine.dll)

# Whisper (for speech-to-text)
# .dll.a is the import library for MinGW (like .lib for Visual Studio)
set(WHISPER_LIB ${CMAKE_CURRENT_SOURCE_DIR}/lib/whisper/libwhisper.dll.a)
# .dll is the actual runtime library
set(WHISPER_DLL ${CMAKE_CURRENT_SOURCE_DIR}/lib/whisper/libwhisper.dll)


# --- Add Executable Target ---
# This defines the name of your final .exe file and lists its source code files.
add_executable(${PROJECT_NAME}
        src/main.cpp
        src/whisper.cpp
        src/Config.cpp
        src/OllamaClient.cpp
        src/intent/IntentClassifier.cpp
)


# --- Include Directories ---
# Tells the compiler where to find header files (#include "...")
target_include_directories(${PROJECT_NAME} PUBLIC
        # For miniaudio.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/nlohmann
        # For our own whisper.h wrapper and the copied whisper_c_api.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


# --- Link Libraries ---
# Tells the linker which libraries to connect to the executable.
target_link_libraries(${PROJECT_NAME} PRIVATE
        ${PORCUPINE_LIB}
        ${WHISPER_LIB}
        ole32
        winmm
        ws2_32
        crypt32
)


# --- Post-Build Step: Copy DLLs to Output Directory ---
# This is crucial. It copies the .dll files next to your .exe so it can find
# them when you run it.

# Copy Porcupine DLL
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PORCUPINE_LIB}" # Note: We use the Porcupine .dll for both linking and runtime with MinGW
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

# Copy Whisper DLL
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WHISPER_DLL}"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

# --- Status Messages for Debugging ---
message(STATUS "Porcupine library path set to: ${PORCUPINE_LIB}")
message(STATUS "Whisper import library path set to: ${WHISPER_LIB}")