#include "loki/core/OllamaClient.h"
#include "httplib/httplib.h"
#include "nlohmann/json.hpp"
#include <iostream>

using json = nlohmann::json;

// Helper to parse host and port from a URL like "http://localhost:11434"
void parse_host_and_port(const std::string &full_host, std::string &address, int &port, bool &is_https) {
    std::string temp = full_host;
    if (temp.rfind("https://", 0) == 0) {
        temp = temp.substr(8);
        is_https = true;
    } else if (temp.rfind("http://", 0) == 0) {
        temp = temp.substr(7);
        is_https = false;
    }

    size_t colon_pos = temp.find(':');
    if (colon_pos != std::string::npos) {
        address = temp.substr(0, colon_pos);
        port = std::stoi(temp.substr(colon_pos + 1));
    } else {
        address = temp;
        port = is_https ? 443 : 80; // Default ports
    }
}

OllamaClient::OllamaClient(const std::string &host, const std::string &model_name, const json& options)
    : model_name_(model_name), options_(options) {
    std::string address;
    int port;
    bool is_https;

    parse_host_and_port(host, address, port, is_https);

    // Note: httplib will automatically handle the protocol based on the object type
    // We assume HTTP for local Ollama. If you need HTTPS, you would use httplib::SSLClient
    if (is_https) {
        // For simplicity, we are not including SSL client setup.
        // The default Ollama server runs on HTTP.
        std::cerr << "WARNING: HTTPS is not fully supported in this client example. Using HTTP logic." << std::endl;
    }

    client_ = std::make_unique<httplib::Client>(address, port);
    client_->set_connection_timeout(5); // 5 seconds to connect
    client_->set_read_timeout(300); // 5 minutes to read response
}

// Destructor must be defined here in the .cpp file where httplib::Client is a complete type
OllamaClient::~OllamaClient() = default;


std::string OllamaClient::generate(const std::string& system_prompt, const std::string& user_prompt) {
    json payload = {
        {"model", model_name_},
        {"system", system_prompt},
        {"prompt", user_prompt},
        {"stream", false}
    };

    if (!options_.is_null() && !options_.empty()) {
        payload["options"] = options_;
    }


    auto res = client_->Post("/api/generate", payload.dump(), "application/json");

    if (!res) {
        auto err = res.error();
        std::string err_str = "Unknown error";
        // httplib::to_string is not a public function, so we map common errors
        if (err == httplib::Error::Connection) err_str = "Connection error";
        else if (err == httplib::Error::Read) err_str = "Read error";
        else if (err == httplib::Error::Write) err_str = "Write error";

        std::cerr << "Ollama request failed: " << err_str << std::endl;
        return "[Error: Could not connect to Ollama server]";
    }

    if (res->status != 200) {
        std::cerr << "Ollama API returned status " << res->status << std::endl;
        std::cerr << "Response body: " << res->body << std::endl;
        return "[Error: Ollama API returned status " + std::to_string(res->status) + "]";
    }

    try {
        json response_json = json::parse(res->body);
        if (response_json.contains("response")) {
            return response_json["response"].get<std::string>();
        }
        if (response_json.contains("error")) {
            std::string error_msg = response_json["error"].get<std::string>();
            std::cerr << "Ollama API error: " << error_msg << std::endl;
            return "[Ollama Error: " + error_msg + "]";
        }
    } catch (const json::parse_error &e) {
        std::cerr << "Failed to parse Ollama JSON response: " << e.what() << std::endl;
        return "[Error: Failed to parse Ollama response]";
    }

    return "[Error: Unknown response format from Ollama]";
}
