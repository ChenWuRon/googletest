#pragma once

// Rule Builder (architecture.md)
// Transforms ConfigDomain into executable runtime rules.

#include <memory>

#include "resource_manager/core/config_domain.h"
#include "resource_manager/error/error.h"

namespace resource_manager {

class RuleBuilder {
public:
    RuleBuilder() = default;
    ~RuleBuilder() = default;

    struct RuntimeRules {
        // Placeholder — defined during Phase 2 implementation
    };

    std::optional<RuntimeRules> build(const ConfigDomain& domain);
};

} // namespace resource_manager
