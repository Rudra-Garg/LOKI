#ifndef LOKI_INTENTCLASSIFIER_H
#define LOKI_INTENTCLASSIFIER_H

#include <string>
#include "nlohmann/json.hpp"
#include "OllamaClient.h" // We need to know about OllamaClient

class IntentClassifier {
public:
    // The structured intent format, as defined in your roadmap
    struct Intent {
        std::string type = "unknown";       // e.g., "system_control", "search", "general"
        std::string action = "";            // e.g., "set_volume", "launch_application"
        nlohmann::json parameters = nlohmann::json::object(); // Extracted parameters
        float confidence = 0.0f;          // 0.0 - 1.0 confidence score from the LLM
    };

    // Constructor takes a reference to an existing OllamaClient
    explicit IntentClassifier(OllamaClient& ollama_client);

    // The main function of this class: takes text, returns a structured Intent
    Intent classify(const std::string& transcript);

private:
    OllamaClient& ollama_client_; // Use a reference, don't own the client
    std::string system_prompt_;   // We'll build a powerful prompt for classification
};

#endif //LOKI_INTENTCLASSIFIER_H