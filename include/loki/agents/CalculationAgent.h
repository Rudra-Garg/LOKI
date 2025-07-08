#pragma once

#include "loki/agents/IAgent.h"

/**
 * @class CalculationAgent
 * @brief An agent responsible for evaluating mathematical expressions.
 */
class CalculationAgent : public IAgent {
public:
    std::string get_name() const override;

    std::string execute(const loki::intent::Intent &intent) override;
};
