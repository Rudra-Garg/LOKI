# LOKI - Voice Assistant

LOKI is a powerful, locally-running voice assistant built in C++ that combines advanced AI technologies to provide a comprehensive voice interaction experience. It features wake word detection, speech-to-text processing, intent classification, and intelligent responses through local LLM integration.

## Features

### 🎙️ **Voice Interaction**
- **Wake Word Detection**: Responds to "Hey Loki" using Porcupine
- **Speech Recognition**: High-quality speech-to-text using Whisper
- **Continuous Listening**: Background processing with system tray integration

### 🤖 **AI-Powered Capabilities**
- **Intent Classification**: Smart understanding of user commands using embedding models
- **Local LLM Integration**: Ollama integration for intelligent responses
- **Conversation**: Natural language conversations and storytelling
- **Context Awareness**: Maintains conversation context

### 🔧 **System Control**
- **Application Management**: Launch browsers, text editors, calculators, and more
- **Volume Control**: Adjust system volume, mute/unmute
- **Power Management**: Shutdown, restart, sleep, and logout commands
- **File System**: Open file explorer and navigate directories

### 📊 **Utility Functions**
- **Mathematical Calculations**: Evaluate complex expressions and equations
- **Time & Date**: Get current time, date, and calendar information
- **Weather Information**: Weather forecasts and current conditions
- **Web Search**: Search the internet and find information

### 🖥️ **User Interface**
- **System Tray Integration**: Runs quietly in the background
- **Qt6 GUI**: Modern, responsive interface for status and logs
- **Real-time Feedback**: Visual status updates and response display
- **Auto-hide Interface**: Minimizes automatically after responses

## Prerequisites

### System Requirements
- **Operating System**: Windows 10/11 (MSVC optimized), Linux (experimental support)
- **RAM**: Minimum 8GB, recommended 16GB
- **CPU**: Modern multi-core processor
- **Storage**: 2GB free space for models and dependencies

### Dependencies
- **Qt6** (Widgets, Core, Gui components)
- **CMake** 3.21 or higher
- **MSVC** (Microsoft Visual C++) - Primary supported compiler
- **Visual Studio 2019/2022** or Build Tools for Visual Studio
- **Ollama** (for LLM functionality)
- **Windows SDK** (for Windows API integration)

### Third-Party Libraries
- **Porcupine** (Picovoice) - Wake word detection
- **Whisper.cpp** - Speech-to-text processing  
- **Llama.cpp** - Local LLM inference
- **GGML** - Machine learning library
- **nlohmann/json** - JSON processing
- **httplib** - HTTP client for Ollama communication
- **miniaudio** - Audio input/output processing
- **TinyExpr** - Mathematical expression evaluation

### Runtime Dependencies (Windows DLLs)
- **ggml.dll, ggml-base.dll, ggml-cpu.dll** - GGML runtime
- **llama.dll** - LLM inference
- **whisper.dll** - Speech recognition
- **pv_porcupine.dll** - Wake word detection
- **Qt6 DLLs** - Automatically deployed by windeployqt

## Installation

### 1. Clone Repository
```bash
git clone https://github.com/Rudra-Garg/LOKI.git
cd LOKI
git submodule update --init --recursive
```

### 2. Install Dependencies

#### Windows (MSVC) - Primary Platform
```bash
# Install Visual Studio 2019/2022 or Build Tools
# Download from: https://visualstudio.microsoft.com/

# Install Qt6 via Qt Online Installer (recommended)
# Download from: https://www.qt.io/download-qt-installer
# Or use vcpkg:
vcpkg install qt6-base qt6-widgets

# Install Ollama
# Download and install from https://ollama.com/

# Ensure CMake is installed and in PATH
# Download from: https://cmake.org/download/
```

#### Linux (Experimental Support)
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install qt6-base-dev qt6-tools-dev cmake build-essential

# Install Ollama
curl -fsSL https://ollama.com/install.sh | sh

# Note: Pre-built libraries are Windows-focused
# Manual compilation of third-party libraries may be required
```

### 3. Download AI Models
Create a `models` directory and download the required models:

```bash
mkdir -p models/whisper models/embedding models/porcupine
```

**Required Models:**
- **Whisper**: `ggml-base.en.bin` (English speech recognition)
- **Embedding**: `all-MiniLM-L6-v2.Q4_K_S.gguf` (intent classification)
- **Porcupine**: `Hey-Loki.ppn` (wake word detection)
- **Vocabulary**: `vocab.txt` (embedding model vocabulary)

### 4. Configure Environment
Create a `.env` file in the project root:

```env
# Porcupine Configuration
ACCESS_KEY=your_porcupine_access_key_here
PORCUPINE_MODEL_PATH=./third-party/picovoice/lib/porcupine_params.pv
KEYWORD_PATH=./models/porcupine/Hey-Loki.ppn
SENSITIVITY=0.5

# Whisper Configuration
WHISPER_MODEL_PATH=./models/whisper/ggml-base.en.bin
MIN_COMMAND_MS=300
VAD_THRESHOLD=0.01

# Embedding Model Configuration
EMBEDDING_MODEL_PATH=./models/embedding/all-MiniLM-L6-v2.Q4_K_S.gguf

# Intent Classification
INTENTS_JSON_PATH=./data/intents.json

# Ollama Configuration
OLLAMA_HOST=http://localhost:11434
OLLAMA_MODEL=dolphin-phi
```

### 5. Build Project

#### Windows (MSVC)
```bash
# Create build directory
mkdir build
cd build

# Configure with MSVC (ensure Visual Studio tools are in PATH)
cmake .. -G "Visual Studio 17 2022" -A x64
# Or for Visual Studio 2019:
# cmake .. -G "Visual Studio 16 2019" -A x64

# Build the project
cmake --build . --config Release

# The executable will be in: build/Release/loki.exe
# All required DLLs are automatically copied to the output directory
```

#### Linux (Experimental)
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release

# Note: May require manual setup of third-party libraries
```

## Usage

### Starting LOKI
1. **Start Ollama**: Ensure Ollama is running with your preferred model
   ```bash
   ollama serve
   ollama pull dolphin-phi  # or your preferred model
   ```

2. **Launch LOKI**: Run the executable from the build directory
   ```bash
   # Windows
   build\Release\loki.exe
   
   # Linux (experimental)
   ./build/loki
   ```

3. **System Tray**: LOKI will appear in your system tray and begin listening for the wake word

### Voice Commands

#### System Control
- "Hey Loki, open browser" - Launch default web browser
- "Hey Loki, open notepad" - Launch text editor
- "Hey Loki, turn the volume up" - Increase system volume
- "Hey Loki, shut down the computer" - Shutdown system

#### Calculations
- "Hey Loki, what is 25 times 47?" - Mathematical calculations
- "Hey Loki, calculate the square root of 144" - Complex math operations

#### Information
- "Hey Loki, what time is it?" - Current time
- "Hey Loki, what's today's date?" - Current date
- "Hey Loki, what's the weather like?" - Weather information

#### Conversation
- "Hey Loki, tell me a joke" - Entertainment
- "Hey Loki, how are you?" - Casual conversation
- "Hey Loki, tell me a story" - Creative content

#### Web Search
- "Hey Loki, search for artificial intelligence" - Web search
- "Hey Loki, look up recipe for chocolate cake" - Information lookup

### GUI Interface
- **Status Window**: Shows real-time status and processing information
- **Response Display**: Presents LOKI's responses and confirmations
- **Auto-hide**: Interface automatically minimizes after displaying responses
- **System Tray**: Right-click for options menu and quit functionality

## Configuration

### Intent Customization
Edit `data/intents.json` to customize voice command recognition:

```json
{
  "type": "system_control",
  "action": "launch_application",
  "prompts": [
    "open browser",
    "launch chrome",
    "start firefox"
  ]
}
```

### Model Configuration
- **Whisper Models**: Replace with different Whisper models for other languages
- **Embedding Models**: Use different embedding models for improved intent classification
- **Wake Word**: Create custom wake words with Porcupine Console

### Ollama Integration
Configure different LLM models:
```bash
ollama pull llama2          # Use Llama 2
ollama pull codellama       # Use CodeLlama
ollama pull mistral         # Use Mistral
```

Update `.env` file with your preferred model:
```env
OLLAMA_MODEL=llama2
```

## Technical Architecture

LOKI is a multi-threaded Qt6 application designed with a focus on Windows/MSVC compatibility. The architecture consists of:

### Core Components
- **LokiWorker**: Main processing thread handling audio input and AI inference
- **AgentManager**: Coordinates specialized agents for different functionality
- **Intent Classification System**: Fast and advanced classifiers for command understanding
- **GUI Integration**: Qt6-based interface with system tray functionality

### Agent-Based System
- **SystemControlAgent**: Windows system integration (applications, volume, power)
- **CalculationAgent**: Mathematical expression evaluation using TinyExpr
- **Extensible Architecture**: Easy addition of new specialized agents

### MSVC Build Optimizations
- **Compiler-Specific Linking**: Automatic .lib vs shared library selection
- **Runtime Asset Management**: Automatic DLL and model file deployment
- **Windows API Integration**: Native system control and audio device access
- **Qt Deployment**: Automated windeployqt integration for distribution

### Audio Processing Pipeline
1. **Wake Word Detection**: Porcupine continuously monitors for "Hey Loki"
2. **Voice Activity Detection**: Automatic speech start/end detection
3. **Speech Recognition**: Whisper.cpp converts speech to text
4. **Intent Classification**: Embedding-based classification with FastClassifier
5. **Agent Execution**: Appropriate agent handles the classified command
6. **Response Generation**: Ollama LLM generates contextual responses

## Project Structure

```
LOKI/
├── src/
│   ├── main.cpp                    # Application entry point
│   ├── AgentManager.cpp            # Coordinates different agent types
│   ├── agents/                     # Specialized functionality agents
│   │   ├── CalculationAgent.cpp    # Mathematical calculations
│   │   └── SystemControlAgent.cpp  # System control operations
│   ├── core/                       # Core application logic
│   │   ├── Config.cpp              # Configuration management
│   │   ├── EmbeddingModel.cpp      # Text embedding processing
│   │   ├── LokiWorker.cpp          # Main worker thread
│   │   ├── OllamaClient.cpp        # LLM integration
│   │   └── Whisper.cpp             # Speech recognition
│   ├── gui/                        # User interface
│   │   └── MainWindow.cpp          # Qt6 main window
│   └── intent/                     # Intent classification
│       ├── FastClassifier.cpp      # Fast intent classification
│       └── IntentClassifier.cpp    # Advanced intent processing
├── include/loki/                   # Header files
├── data/
│   └── intents.json               # Intent definitions
├── third-party/                   # External libraries
├── models/                        # AI models (not in repo)
└── CMakeLists.txt                 # Build configuration
```

## Development

### Building from Source

#### Windows (MSVC) - Recommended
```bash
# Debug build
mkdir build-debug
cd build-debug
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug

# Release build
mkdir build-release  
cd build-release
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

#### Alternative Compilers (Experimental)
```bash
# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --config Debug

# Release build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

### Adding New Agents
1. Create new agent class in `src/agents/`
2. Implement agent interface
3. Register agent in `AgentManager`
4. Add corresponding intents to `data/intents.json`

### Custom Wake Words
1. Create custom wake word model using Porcupine Console
2. Place `.ppn` file in `models/porcupine/`
3. Update `KEYWORD_PATH` in `.env`

## Troubleshooting

### Common Issues

**Audio Device Problems**
- Ensure microphone permissions are granted
- Check default audio input device
- Verify microphone is not muted

**Model Loading Errors**
- Verify model files are in correct directories
- Check file permissions
- Ensure sufficient disk space

**Ollama Connection Issues**
- Verify Ollama is running: `ollama serve`
- Check Ollama host configuration in `.env`
- Ensure model is downloaded: `ollama pull model-name`

**Build Errors**
- **MSVC**: Verify Visual Studio 2019/2022 is installed with C++ tools
- Check CMake version (3.21+ required)
- Ensure Qt6 is properly configured and in PATH
- Verify Windows SDK is installed for system API integration

**MSVC-Specific Issues**
- Ensure `/FI` forced include for `msvc_compat.h` is working
- Verify .lib files are correctly linked (not .dll files)
- Check that `ws2_32.lib` is available for Windows sockets
- Use Visual Studio Developer Command Prompt if build fails

**Missing DLL Errors**
- Ensure all required DLLs are copied to output directory
- Check CMakeLists.txt `COPY_RUNTIME_ASSET` commands completed successfully  
- Verify `windeployqt` ran successfully for Qt dependencies
- Manually copy missing DLLs from `third-party/*/lib/` if needed

### Performance Optimization
- Use GPU acceleration if available
- Adjust model sizes based on system capabilities
- Configure thread counts for optimal performance
- Monitor memory usage during operation

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- **Picovoice** for Porcupine wake word detection
- **OpenAI** for Whisper speech recognition
- **Ollama** for local LLM integration
- **Qt** for the user interface framework
- **Contributors** who have helped improve LOKI

---

**Note**: This is a local-first voice assistant that prioritizes privacy by running all AI processing on your device. No voice data is transmitted to external servers unless explicitly configured for web search functionality.