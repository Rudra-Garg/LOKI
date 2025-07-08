#include "loki/intent/FastClassifier.h"
#include <iostream>
#include <numeric>
#include <cmath>
#include <string>
#include <cctype>
#include <algorithm>
#include <fstream>

// Helper function to calculate cosine similarity between two vectors
float cosine_similarity(const std::vector<float> &a, const std::vector<float> &b) {
    if (a.size() != b.size() || a.empty()) {
        return 0.0f;
    }
    float dot_product = std::inner_product(a.begin(), a.end(), b.begin(), 0.0f);
    float norm_a = std::sqrt(std::inner_product(a.begin(), a.end(), a.begin(), 0.0f));
    float norm_b = std::sqrt(std::inner_product(b.begin(), b.end(), b.begin(), 0.0f));

    if (norm_a == 0.0f || norm_b == 0.0f) {
        return 0.0f;
    }
    return dot_product / (norm_a * norm_b);
}

namespace loki {
    namespace intent {
        std::string normalize_text(const std::string &input) {
            std::string output;
            output.reserve(input.length());
            for (char c: input) {
                if (!std::ispunct(static_cast<unsigned char>(c))) {
                    output += std::tolower(static_cast<unsigned char>(c));
                }
            }
            return output;
        }

        FastClassifier::FastClassifier(const std::string &intents_path, EmbeddingModel &embedding_model)
            : embedding_model_(embedding_model) {
            std::cout << "Loading intents from '" << intents_path << "'..." << std::endl;
            std::ifstream intents_file(intents_path);
            if (!intents_file.is_open()) {
                throw std::runtime_error("Failed to open intents file: " + intents_path);
            }

            nlohmann::json intents_json = nlohmann::json::parse(intents_file);

            std::cout << "Pre-computing embeddings for known intents..." << std::endl;

            // This loop replaces the hardcoded vector entirely.
            for (const auto &intent_group: intents_json) {
                std::string type = intent_group.at("type");
                std::string action = intent_group.at("action");

                for (const auto &prompt: intent_group.at("prompts")) {
                    std::string prompt_str = prompt.get<std::string>();
                    std::string normalized_prompt = normalize_text(prompt_str);

                    KnownIntent known_intent;
                    known_intent.text_prompt = prompt_str;
                    known_intent.type = type;
                    known_intent.action = action;
                    known_intent.embedding = embedding_model_.get_embeddings(normalized_prompt);

                    known_intents_.push_back(std::move(known_intent));
                }
            }

            std::cout << "FastClassifier is ready with " << known_intents_.size() << " training prompts." << std::endl;
        }

        FastClassifier::ClassificationResult FastClassifier::classify(const std::string &transcript) const {
            ClassificationResult result;
            if (transcript.empty()) return result;

            std::string normalized_transcript = normalize_text(transcript);
            std::vector<float> transcript_embedding = embedding_model_.get_embeddings(normalized_transcript);

            float best_score = 0.0f;
            const KnownIntent *best_match = nullptr;

            for (const auto &known_intent: known_intents_) {
                float score = cosine_similarity(transcript_embedding, known_intent.embedding);
                if (score > best_score) {
                    best_score = score;
                    best_match = &known_intent;
                }
            }

            if (best_match && best_score >= SIMILARITY_THRESHOLD) {
                result.has_match = true;
                result.confidence = best_score;
                result.type = best_match->type;
                result.action = best_match->action;
                if (result.action == "launch_application") {
                    if (normalized_transcript.find("chrome") != std::string::npos || normalized_transcript.find(
                            "browser") !=
                        std::string::npos) {
                        result.parameters["name"] = "chrome";
                    } else if (normalized_transcript.find("firefox") != std::string::npos) {
                        result.parameters["name"] = "firefox";
                    } else if (normalized_transcript.find("notepad") != std::string::npos) {
                        result.parameters["name"] = "notepad";
                    } // ... etc.
                } else if (result.action == "set_volume") {
                    if (normalized_transcript.find("up") != std::string::npos || normalized_transcript.find("increase")
                        !=
                        std::string::npos || normalized_transcript.find("louder") != std::string::npos) {
                        result.parameters["direction"] = "up";
                    } else if (normalized_transcript.find("down") != std::string::npos || normalized_transcript.find(
                                   "decrease")
                               != std::string::npos || normalized_transcript.find("quieter") != std::string::npos) {
                        result.parameters["direction"] = "down";
                    } else if (normalized_transcript.find("mute") != std::string::npos) {
                        result.parameters["direction"] = "mute";
                    }
                }
            }

            return result;
        }
    } // namespace intent
} // namespace loki
