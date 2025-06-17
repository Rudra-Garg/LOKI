#pragma once
#include "loki/agents/IAgent.h"
#include <memory>
#include <string>
#include <unordered_map>

class AgentManager {
public:
    void register_agent(std::unique_ptr<IAgent> agent);

    std::string dispatch(const intent::Intent &intent);

private:
    std::unordered_map<std::string, std::unique_ptr<IAgent> > agents_;
};
