#ifndef LOKI_OLLAMACLIENT_H
#define LOKI_OLLAMACLIENT_H

#include <string>
#include <memory>

// Forward declare to hide implementation details (httplib::Client) from the header
namespace httplib { class Client; }

class OllamaClient {
public:
    // Constructor initializes the client with the Ollama server details.
    OllamaClient(const std::string& host, const std::string& model_name);
    // Destructor is required for the PIMPL-lite pattern with unique_ptr
    ~OllamaClient();

    // Sends a prompt to the Ollama model and returns the response.
    std::string generate(const std::string& prompt);

private:
    std::string model_name_;
    std::unique_ptr<httplib::Client> client_;
};

#endif //LOKI_OLLAMACLIENT_H