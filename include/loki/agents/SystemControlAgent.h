#pragma once

#include "loki/agents/IAgent.h"
#include <memory>

namespace loki::agents {
    class ISystemControl;
} // Forward declaration

/**
 * @class SystemControlAgent
 * @brief An agent responsible for OS-level actions like launching applications.
 *        It delegates the actual system calls to an ISystemControl implementation.
 */
class SystemControlAgent : public IAgent {
public:
    // Constructor now accepts a system control implementation.
    // This allows for dependency injection, making the agent testable.
    // If no implementation is provided, it defaults to the platform-specific one.
    explicit SystemControlAgent(std::shared_ptr<loki::agents::ISystemControl> sys_control = nullptr);

    std::string get_name() const override;

    std::string execute(const loki::intent::Intent &intent) override;

private:
    std::shared_ptr<loki::agents::ISystemControl> sys_control_;
};
