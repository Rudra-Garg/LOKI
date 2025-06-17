#include "loki/AgentManager.h"
#include <iostream>

void AgentManager::register_agent(std::unique_ptr<IAgent> agent) {
    if (agent) {
        std::cout << "Registering agent: " << agent->get_name() << std::endl;
        agents_[agent->get_name()] = std::move(agent);
    }
}

std::string AgentManager::dispatch(const intent::Intent &intent) {
    auto it = agents_.find(intent.type);
    if (it != agents_.end()) {
        return it->second->execute(intent);
    }
    std::cerr << "WARNING: No agent registered for intent type '" << intent.type << "'" << std::endl;
    return "I'm not sure how to handle that request.";
}
