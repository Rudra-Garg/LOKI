#include "Config.h"
#include <fstream>
#include <iostream>
#include <algorithm> // For std::remove

// Helper function to trim leading/trailing whitespace from a string
std::string trim(const std::string &s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, (end - start + 1));
}

Config::Config(const std::string &env_path) {
    std::ifstream file(env_path);
    if (!file.is_open()) {
        std::cout << "INFO: .env file not found at '" << env_path <<
                "'. Using default values and environment variables." << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Remove comments and trim whitespace
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        line = trim(line);

        if (line.empty()) {
            continue;
        }

        auto delimiter_pos = line.find('=');
        if (delimiter_pos != std::string::npos) {
            std::string key = trim(line.substr(0, delimiter_pos));
            std::string value = trim(line.substr(delimiter_pos + 1));

            // Optional: remove quotes from value if they exist
            if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }

            if (!key.empty()) {
                data[key] = value;
            }
        }
    }
}

std::string Config::get(const std::string &key, const std::string &default_value) const {
    auto it = data.find(key);
    if (it != data.end()) {
        return it->second;
    }
    return default_value;
}

float Config::get_float(const std::string &key, float default_value) const {
    auto it = data.find(key);
    if (it != data.end()) {
        try {
            return std::stof(it->second);
        } catch (const std::exception &e) {
            std::cerr << "WARNING: Could not convert config value '" << it->second << "' for key '" << key <<
                    "' to float. Using default." << std::endl;
            return default_value;
        }
    }
    return default_value;
}
