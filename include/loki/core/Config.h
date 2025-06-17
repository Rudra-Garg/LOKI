#ifndef LOKI_CONFIG_H
#define LOKI_CONFIG_H

#include <string>
#include <map>

// A simple class to load and access key-value pairs from a .env file.
class Config {
public:
    // Constructor loads settings from the specified file path.
    explicit Config(const std::string& env_path = ".env");

    // Gets a string value for a given key.
    // Returns a default value if the key is not found.
    std::string get(const std::string& key, const std::string& default_value = "") const;

    // Gets a float value for a given key.
    float get_float(const std::string& key, float default_value = 0.0f) const;

private:
    std::map<std::string, std::string> data;
};

#endif //LOKI_CONFIG_H