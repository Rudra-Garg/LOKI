#pragma once
#include "nlohmann/json.hpp"
#include <string>

namespace loki::intent {
    struct Intent {
        std::string type = "unknown";
        std::string action;
        nlohmann::json parameters = nlohmann::json::object();
        float confidence = 0.0f;
    };
}
