#pragma once

#include "loki/core/EmbeddingModel.h"
#include "loki/intent/Intent.h"
#include <vector>
#include <string>

// Keep this struct definition as it represents a single, processed training example.
struct KnownIntent {
    std::string text_prompt; // The original text for debugging/reference
    std::vector<float> embedding;
    std::string type;
    std::string action;
};

namespace loki {
    namespace intent {
        class FastClassifier {
        public:
            struct ClassificationResult {
                bool has_match = false;
                float confidence = 0.0f;
                std::string type;
                std::string action;
                nlohmann::json parameters;
            };

            // MODIFIED: The constructor now takes the path to the intents file and
            // a reference to the already-created EmbeddingModel. This is better design
            // (Dependency Injection).
            FastClassifier(const std::string &intents_path, EmbeddingModel &embedding_model);

            ClassificationResult classify(const std::string &transcript) const;

        private:
            std::vector<KnownIntent> known_intents_;
            EmbeddingModel &embedding_model_; // Store a reference, don't own it.
            const float SIMILARITY_THRESHOLD = 0.85f; // Lowered slightly for more flexibility
        };
    } // namespace intent
} // namespace loki
