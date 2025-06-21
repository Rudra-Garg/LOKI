#include "loki/intent/IntentClassifier.h"
#include <iostream>

IntentClassifier::IntentClassifier(OllamaClient &ollama_client)
    : ollama_client_(ollama_client) {
    // This is the prompt engineering heart of the controller LLM.
    // It's much stricter than our previous prompt.
    system_prompt_ = R"(
You are a non‑conversational API. Your sole job is to read the user’s utterance and emit exactly one valid JSON object—nothing else.

RESPONSE RULES:
1. Output **only** a single, raw JSON object. No markdown, no explanations, no extra keys.
2. JSON **must** contain exactly these four keys, in any order:
   • "type"       – one of: "system_control", "search", "general", "calculation", "unknown"
   • "action"     – see schema below
   • "parameters" – an object ({} if none)
   • "confidence" – a float between 0.0 and 1.0
3. If you’re not confident (>0.2) or don’t understand, return:
   {"type":"unknown","action":"","parameters":{},"confidence":0.1}

SCHEMA:
• type="system_control":
    actions: "set_volume", "launch_application", "close_application"
• type="search":
    action: "web_search"
• type="general":
    actions: "get_time", "conversation"
• type="calculation":
    action: "evaluate_expression"

EXAMPLES:
User: "launch chrome for me"
{"type":"system_control","action":"launch_application","parameters":{"name":"chrome"},"confidence":1.0}

User: "search for pictures of cats"
{"type":"search","action":"web_search","parameters":{"query":"pictures of cats"},"confidence":1.0}

User: "what time is it?"
{"type":"general","action":"get_time","parameters":{},"confidence":1.0}

User: "calculate 5 times 8"
{"type":"calculation","action":"evaluate_expression","parameters":{"expression":"5 * 8"},"confidence":1.0}

User: "fsdjakl fjdsa"
{"type":"unknown","action":"","parameters":{},"confidence":0.1}
)";
}


intent::Intent IntentClassifier::classify(const std::string &transcript) {
    std::cout << "--- Classifying intent for: \"" << transcript << "\"" << std::endl;
    std::string llm_response_str = ollama_client_.generate(system_prompt_, transcript);

    intent::Intent result_intent;
    std::string json_to_parse = llm_response_str;
    size_t first_brace = llm_response_str.find('{');
    size_t last_brace = llm_response_str.rfind('}');

    if (first_brace != std::string::npos && last_brace != std::string::npos && last_brace > first_brace) {
        json_to_parse = llm_response_str.substr(first_brace, last_brace - first_brace + 1);
    }
    try {
        using json = nlohmann::json;
        json parsed = json::parse(json_to_parse);

        // Use .at() for required fields, which throws an exception if the key is missing.
        result_intent.type = parsed.at("type").get<std::string>();
        result_intent.confidence = parsed.at("confidence").get<float>();
        result_intent.parameters = parsed.at("parameters");

        // Use .value() for optional fields, providing a default value.
        result_intent.action = parsed.value("action", "");
    } catch (const nlohmann::json::exception &e) {
        std::cerr << "ERROR: Failed to parse or validate LLM JSON response. " << e.what() << std::endl;
        std::cerr << "Raw response was: " << llm_response_str << std::endl;

        // Return a safe, 'unknown' intent on any failure
        result_intent.type = "unknown";
        result_intent.confidence = 0.0f;
        result_intent.action = "";
        result_intent.parameters = nlohmann::json::object();
    }

    return result_intent;
}
