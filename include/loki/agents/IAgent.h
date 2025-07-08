#pragma once

#include "loki/intent/Intent.h" // Includes the loki::intent::Intent struct
#include <string>

/*
 * @class IAgent
 * @brief An abstract base class defining the interface for all agents in LOKI.
 *
 * Each agent is responsible for handling a specific 'type' of intent (e.g., "system_control").
 * The AgentManager uses this common interface to dispatch intents without needing
 * to know the concrete details of each agent.
 */
class IAgent {
public:
    // Virtual destructor is essential for base classes with virtual functions.
    virtual ~IAgent() = default;

    /**
     * @brief Gets the unique name of the agent.
     * @return The name, which must match an intent 'type' (e.g., "system_control").
     */
    virtual std::string get_name() const = 0;

    /**
     * @brief Executes the action specified in the intent.
     * @param intent The structured intent object from the classifier.
     * @return A string response for LOKI to speak back to the user.
     */
    virtual std::string execute(const loki::intent::Intent &intent) = 0;
};
