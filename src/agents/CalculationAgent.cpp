#include "loki/agents/CalculationAgent.h"
#include "tinyexpr/tinyexpr.h"
#include <iostream>
#include <sstream>

std::string CalculationAgent::get_name() const {
    // This name MUST match the "type" from the IntentClassifier.
    return "calculation";
}

std::string CalculationAgent::execute(const intent::Intent &intent) {
    if (intent.action == "evaluate_expression") {
        if (!intent.parameters.contains("expression")) {
            return "You asked me to calculate something, but didn't provide an expression.";
        }

        std::string expression = intent.parameters["expression"];
        std::cout << "AGENT_LOG: Evaluating expression: '" << expression << "'" << std::endl;

        int error;
        // Use the tinyexpr library to safely evaluate the expression.
        double result = te_interp(expression.c_str(), &error);

        if (error) {
            std::cerr << "AGENT_ERROR: tinyexpr failed at position " << error << std::endl;
            return "I'm sorry, I couldn't understand that math expression.";
        } else {
            // Use a stringstream to format the number nicely, avoiding trailing zeros.
            std::ostringstream ss;
            ss << result;
            return "The answer is " + ss.str();
        }
    }

    return "I don't know how to perform that calculation.";
}
