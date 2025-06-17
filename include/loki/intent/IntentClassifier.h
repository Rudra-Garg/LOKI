#ifndef LOKI_INTENTCLASSIFIER_H
#define LOKI_INTENTCLASSIFIER_H

#include <string>

#include "Intent.h"
#include "nlohmann/json.hpp"
#include "../core/OllamaClient.h"

class IntentClassifier {
public:

    // Constructor takes a reference to an existing OllamaClient
    explicit IntentClassifier(OllamaClient& ollama_client);

    // The main function of this class: takes text, returns a structured Intent
    intent::Intent classify(const std::string& transcript);

private:
    OllamaClient& ollama_client_; // Use a reference, don't own the client
    std::string system_prompt_;   // We'll build a powerful prompt for classification
};

#endif //LOKI_INTENTCLASSIFIER_H