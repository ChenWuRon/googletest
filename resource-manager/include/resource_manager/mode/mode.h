#pragma once

// ADR-004 Mode System
// Mode is a triplet: (service, namespace, entity).
// uint32_t mode is explicitly forbidden.

#include <string>

namespace resource_manager {

enum class ServiceType {
    None,
    Systemd,
    Custom,
};

enum class NamespaceType {
    None,
    Cgroup,
    Custom,
};

enum class EntityType {
    None,
    ProcessName,
    CommandLine,
    ExecutablePath,
};

struct Mode {
    ServiceType service = ServiceType::None;
    NamespaceType ns = NamespaceType::None;
    EntityType entity = EntityType::None;
};

bool operator==(const Mode& lhs, const Mode& rhs);
bool operator!=(const Mode& lhs, const Mode& rhs);

} // namespace resource_manager
