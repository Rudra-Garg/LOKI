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
- **Operating System**: Windows 10/11 (primary), Linux (experimental)
- **RAM**: Minimum 8GB, recommended 16GB
- **CPU**: Modern multi-core processor
- **Storage**: 2GB free space for models and dependencies

### Dependencies
- **Qt6** (Widgets, Core, Gui components)
- **CMake** 3.21 or higher
- **C++17** compatible compiler (MSVC, GCC, or Clang)
- **Ollama** (for LLM functionality)

### Third-Party Libraries
- **Porcupine** (Picovoice) - Wake word detection
- **Whisper.cpp** - Speech-to-text processing
- **Llama.cpp** - Local LLM inference
- **GGML** - Machine learning library
- **nlohmann/json** - JSON processing
- **httplib** - HTTP client
- **miniaudio** - Audio processing
- **TinyExpr** - Mathematical expression evaluation

## Installation

### 1. Clone Repository
```bash
git clone https://github.com/Rudra-Garg/LOKI.git
cd LOKI
git submodule update --init --recursive
```

### 2. Install Dependencies

#### Windows (MSVC)
```bash
# Install Qt6 via Qt Online Installer or vcpkg
vcpkg install qt6-base qt6-widgets

# Install Ollama
# Download and install from https://ollama.com/
```

#### Linux
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install qt6-base-dev qt6-tools-dev cmake build-essential

# Install Ollama
curl -fsSL https://ollama.com/install.sh | sh
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
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
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
   ./loki              # Linux
   loki.exe            # Windows
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
- Verify all dependencies are installed
- Check CMake version (3.21+ required)
- Ensure Qt6 is properly configured

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