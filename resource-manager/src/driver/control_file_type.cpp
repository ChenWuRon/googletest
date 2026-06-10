#include "resource_manager/driver/control_file_type.h"

#include <mutex>
#include <shared_mutex>

namespace resource_manager {

ControllerRegistry& ControllerRegistry::instance() {
    static ControllerRegistry registry;
    return registry;
}

void ControllerRegistry::registerController(const ControllerDefinition& ctrl) {
    std::unique_lock lock(mutex_);
    registry_[ctrl.name] = ctrl;
}

void ControllerRegistry::registerControlFile(const std::string& controller, const ControlFileType& type) {
    std::unique_lock lock(mutex_);
    registry_[controller].items[type.name] = type;
}

const ControllerDefinition* ControllerRegistry::findController(const std::string& name) const {
    std::shared_lock lock(mutex_);
    auto it = registry_.find(name);
    if (it == registry_.end()) {
        return nullptr;
    }
    return &it->second;
}

const ControlFileType* ControllerRegistry::findControlFile(
    const std::string& controller,
    const std::string& item) const
{
    std::shared_lock lock(mutex_);
    auto cit = registry_.find(controller);
    if (cit == registry_.end()) {
        return nullptr;
    }
    auto iit = cit->second.items.find(item);
    if (iit == cit->second.items.end()) {
        return nullptr;
    }
    return &iit->second;
}

std::vector<std::string> ControllerRegistry::listControllers() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> names;
    names.reserve(registry_.size());
    for (const auto& [name, _] : registry_) {
        names.push_back(name);
    }
    return names;
}

void ControllerRegistry::registerBuiltins() {
    ControllerDefinition cpu;
    cpu.name = "cpu";
    cpu.min_version = 2;
    cpu.max_version = 2;
    cpu.items["cpu.max"] = {"cpu.max", ValueType::Quota, 2, "max 100000",
        {"^(max|\\d+) \\d+$", "", "", {}, true}};
    cpu.items["cpu.weight"] = {"cpu.weight", ValueType::Weight, 2, "100",
        {"^([1-9]\\d{0,3}|10000)$", "", "", {}, false}};
    cpu.items["cpu.weight.nice"] = {"cpu.weight.nice", ValueType::Int, 2, "0",
        {"^(max|\\d+)$", "", "", {}, true}};
    cpu.items["cpu.idle"] = {"cpu.idle", ValueType::Int, 2, "0",
        {"^[01]$", "", "", {}, false}};
    registerController(cpu);

    ControllerDefinition memory;
    memory.name = "memory";
    memory.min_version = 2;
    memory.max_version = 2;
    memory.items["memory.max"] = {"memory.max", ValueType::Size, 2, "max",
        {"^(max|\\d+[KMG]?)$", "", "", {}, true}};
    memory.items["memory.high"] = {"memory.high", ValueType::Size, 2, "max",
        {"^(max|\\d+[KMG]?)$", "", "", {}, true}};
    memory.items["memory.low"] = {"memory.low", ValueType::Size, 2, "0",
        {"^(max|\\d+[KMG]?)$", "", "", {}, true}};
    memory.items["memory.min"] = {"memory.min", ValueType::Size, 2, "0",
        {"^(max|\\d+[KMG]?)$", "", "", {}, true}};
    memory.items["memory.swap.max"] = {"memory.swap.max", ValueType::Size, 2, "max",
        {"^(max|\\d+[KMG]?)$", "", "", {}, true}};
    registerController(memory);

    ControllerDefinition cpuset;
    cpuset.name = "cpuset";
    cpuset.min_version = 2;
    cpuset.max_version = 2;
    cpuset.items["cpuset.cpus"] = {"cpuset.cpus", ValueType::Bitmap, 2, "",
        {"^(\\d+(-\\d+)?)(,\\d+(-\\d+)?)*$", "", "", {}, false}};
    cpuset.items["cpuset.mems"] = {"cpuset.mems", ValueType::Bitmap, 2, "",
        {"^(\\d+(-\\d+)?)(,\\d+(-\\d+)?)*$", "", "", {}, false}};
    cpuset.items["cpuset.cpus.partition"] = {"cpuset.cpus.partition", ValueType::Enum, 2, "member",
        {"", "", "", {"member", "root"}, false}};
    registerController(cpuset);

    ControllerDefinition io;
    io.name = "io";
    io.min_version = 2;
    io.max_version = 2;
    io.items["io.max"] = {"io.max", ValueType::List, 2, "",
        {"^.*$", "", "", {}, false}};
    io.items["io.weight"] = {"io.weight", ValueType::Weight, 2, "100",
        {"^([1-9]\\d{0,3}|10000)$", "", "", {}, false}};
    registerController(io);

    ControllerDefinition pids;
    pids.name = "pids";
    pids.min_version = 2;
    pids.max_version = 2;
    pids.items["pids.max"] = {"pids.max", ValueType::Int, 2, "max",
        {"^(max|\\d+)$", "", "", {}, true}};
    registerController(pids);
}

} // namespace resource_manager
