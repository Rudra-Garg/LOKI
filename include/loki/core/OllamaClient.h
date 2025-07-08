#ifndef LOKI_OLLAMACLIENT_H
#define LOKI_OLLAMACLIENT_H

#include <string>
#include <memory>
#include "nlohmann/json.hpp" // NEW: Include the json header

// Forward declare to hide implementation details (httplib::Client) from the header
namespace httplib {
    class Client;
}

namespace loki {
    namespace core {
        class OllamaClient {
        public:
            // MODIFIED: Add a new constructor that accepts performance options.
            // The default empty json object makes it backwards compatible.
            OllamaClient(const std::string &host, const std::string &model_name, const nlohmann::json &options = {});

            // Destructor is required for the PIMPL-lite pattern with unique_ptr
            ~OllamaClient();

            // Sends a prompt to the Ollama model and returns the response.
            std::string generate(const std::string &system_prompt, const std::string &user_prompt);

        private:
            std::string model_name_;
            nlohmann::json options_; // NEW: Member variable to store the options
            std::unique_ptr<httplib::Client> client_;
        };
    } // namespace core
} // namespace loki

#endif //LOKI_OLLAMACLIENT_H
