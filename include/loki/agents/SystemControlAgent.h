#pragma once

#include "loki/agents/IAgent.h"

/**
 * @class SystemControlAgent
 * @brief An agent responsible for OS-level actions like launching applications.
 */
class SystemControlAgent : public IAgent {
public:
    std::string get_name() const override;

    std::string execute(const loki::intent::Intent &intent) override;
};
