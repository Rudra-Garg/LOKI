#include "loki/core/Whisper.h"

// --- Private Implementation ---
// Include the C API header from its new, private location.
#include "whisper_cpp/include/whisper_c_api.h"

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <memory> // ADDED for std::unique_ptr

// The implementation class that holds the whisper_context and other details.
class Whisper::WhisperImpl {
private:
    whisper_context *ctx_ = nullptr;

public:
    explicit WhisperImpl(const std::string &model_path) {
        // Use the new, non-deprecated function to initialize
        struct whisper_context_params cparams = whisper_context_default_params();
        ctx_ = whisper_init_from_file_with_params(model_path.c_str(), cparams);

        if (ctx_ == nullptr) {
            std::cerr << "Error: failed to initialize whisper model from: " << model_path << std::endl;
            return;
        }
        std::cout << "Whisper initialized with model: " << model_path << std::endl;
    }

    ~WhisperImpl() {
        if (ctx_) {
            whisper_free(ctx_);
            ctx_ = nullptr;
        }
    }

    std::string process_audio(const std::vector<float> &audio_data) {
        if (!ctx_ || audio_data.empty()) {
            return "";
        }

        whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        params.n_threads = std::min(8, (int) std::thread::hardware_concurrency());
        params.print_progress = false;
        params.print_special = false;
        params.print_timestamps = false;
        params.print_realtime = false;
        params.suppress_blank = true;
        params.language = "en";

        if (whisper_full(ctx_, params, audio_data.data(), audio_data.size()) != 0) {
            std::cerr << "Error: failed to process audio with whisper_full" << std::endl;
            return "";
        }

        std::string result_text;
        const int n_segments = whisper_full_n_segments(ctx_);
        for (int i = 0; i < n_segments; ++i) {
            const char *text = whisper_full_get_segment_text(ctx_, i);
            result_text += text;
        }

        if (!result_text.empty()) {
            size_t first = result_text.find_first_not_of(" \t\n\r");
            if (std::string::npos == first) return "";
            size_t last = result_text.find_last_not_of(" \t\n\r");
            result_text = result_text.substr(first, (last - first + 1));
        }

        return result_text;
    }

    [[nodiscard]] bool is_model_loaded() const {
        return ctx_ != nullptr;
    }
};

// --- Public Wrapper Function Implementations ---

// UPDATED: Returns a unique_ptr and manages memory correctly.
std::unique_ptr<Whisper> Whisper::create(const std::string &model_path) {
    // Use `new` because constructor is private. Wrap immediately in unique_ptr.
    auto instance = std::unique_ptr<Whisper>(new Whisper());
    instance->impl_ = new WhisperImpl(model_path);

    if (instance->impl_->is_model_loaded()) {
        return instance; // Move the unique_ptr out.
    }

    // If model loading fails, instance (and its impl_) will be destroyed
    // automatically when the unique_ptr goes out of scope here.
    return nullptr;
}

std::string Whisper::process_audio(const std::vector<float> &audio_data) {
    return impl_ ? impl_->process_audio(audio_data) : "";
}

// REMOVED: destroy() method is gone.

Whisper::Whisper() = default;

// The destructor is already defined here, which is correct for PIMPL.
Whisper::~Whisper() {
    delete impl_;
}