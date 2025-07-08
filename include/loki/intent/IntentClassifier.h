#ifndef LOKI_INTENTCLASSIFIER_H
#define LOKI_INTENTCLASSIFIER_H

#include <string>

#include "Intent.h"
#include "nlohmann/json.hpp"
#include "loki/core/OllamaClient.h"

namespace loki {
    namespace intent {
        class IntentClassifier {
        public:
            // Constructor takes a reference to an existing OllamaClient
            explicit IntentClassifier(loki::core::OllamaClient &ollama_client);

            // The main function of this class: takes text, returns a structured Intent
            loki::intent::Intent classify(const std::string &transcript);

        private:
            loki::core::OllamaClient &ollama_client_; // Use a reference, don't own the client
            std::string system_prompt_; // We'll build a powerful prompt for classification
        };
    } // namespace intent
} // namespace loki

#endif //LOKI_INTENTCLASSIFIER_H
