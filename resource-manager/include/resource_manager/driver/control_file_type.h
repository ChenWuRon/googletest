#pragma once

// ADR-009 Controller Model
// ControlFileType: unified abstraction for all cgroup control files.
// Includes name, value_type, version, default_value.

#include <string>
#include <map>
#include <memory>

#include "resource_manager/core/config_domain.h"

namespace resource_manager {

struct ControlFileType {
    std::string name;
    ValueType value_type;
    int version; // 1 | 2
    std::string default_value;
};

class ControllerRegistry {
public:
    static ControllerRegistry& instance();

    void register_control_file(const std::string& controller, const ControlFileType& type);
    const ControlFileType* get_control_file(const std::string& controller, const std::string& name) const;

    // Built-in controllers (cpu, memory, cpuset, io, pids)
    void register_builtins();

private:
    ControllerRegistry() = default;
    std::map<std::string, std::map<std::string, ControlFileType>> registry_;
};

} // namespace resource_manager
