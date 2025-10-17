#include "loki/agents/CalculationAgent.h"
#include "tinyexpr/tinyexpr.h"
#include <iostream>
#include <sstream>

const std::string LOG_PREFIX = "[CalculationAgent] ";

std::string CalculationAgent::get_name() const {
    // This name MUST match the "type" from the IntentClassifier.
    return "calculation";
}

std::string CalculationAgent::execute(const loki::intent::Intent &intent) {
    if (intent.action == "evaluate_expression") {
        std::cout << LOG_PREFIX << "Executing action 'evaluate_expression'." << std::endl;
        if (!intent.parameters.contains("expression")) {
            std::cerr << LOG_PREFIX << "ERROR: Intent is missing 'expression' parameter." << std::endl;
            return "You asked me to calculate something, but didn't provide an expression.";
        }

        std::string expression = intent.parameters["expression"];
        std::cout << LOG_PREFIX << "Evaluating expression: '" << expression << "'" << std::endl;

        int error;
        // Use the tinyexpr library to safely evaluate the expression.
        double result = te_interp(expression.c_str(), &error);

        if (error) {
            std::cerr << LOG_PREFIX << "ERROR: tinyexpr failed at position " << error << " in expression." << std::endl;
            return "I'm sorry, I couldn't understand that math expression.";
        } else {
            // Use a stringstream to format the number nicely, avoiding trailing zeros.
            std::ostringstream ss;
            ss << result;
            std::string answer = "The answer is " + ss.str();
            std::cout << LOG_PREFIX << "Evaluation successful. Responding with: \"" << answer << "\"" << std::endl;
            return answer;
        }
    }

    std::cerr << LOG_PREFIX << "WARNING: Unhandled action '" << intent.action << "'" << std::endl;
    return "I don't know how to perform that calculation.";
}
