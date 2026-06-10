#include "resource_manager/driver/resource_applier.h"

namespace resource_manager {

ResourceApplier::ResourceApplier(ICgroupDriver& driver, ControllerRegistry& registry)
    : driver_(driver)
    , registry_(registry)
{
}

std::optional<Error> ResourceApplier::apply(const ConfigDomain& domain) {
    for (const auto& [name, child] : domain.root().children()) {
        if (child->type() == ConfigNodeType::GROUP) {
            auto err = applyGroup(*child);
            if (err) return err;
        }
    }
    return std::nullopt;
}

std::optional<Error> ResourceApplier::applyGroup(const ConfigNode& groupNode) {
    const std::string& groupName = groupNode.name();
    std::string cgPath = groupName;

    auto err = driver_.createGroup(cgPath);
    if (err) return err;

    for (const auto& [ctrlName, ctrlChild] : groupNode.children()) {
        if (ctrlChild->type() != ConfigNodeType::CONTROLLER) continue;

        auto ctrlDef = registry_.findController(ctrlName);
        if (!ctrlDef) {
            return Error(ErrorCode::ControllerNotSupported,
                         "Unknown controller: " + ctrlName,
                         "ResourceApplier");
        }

        err = driver_.enableController(cgPath, ctrlName);
        if (err) return err;

        for (const auto& [itemName, itemChild] : ctrlChild->children()) {
            if (itemChild->type() != ConfigNodeType::ITEM) continue;

            auto itemDef = registry_.findControlFile(ctrlName, itemName);
            if (!itemDef) {
                return Error(ErrorCode::InvalidControlValue,
                             "Unknown item: " + itemName + " for controller " + ctrlName,
                             "ResourceApplier");
            }

            err = driver_.setValue(cgPath, itemName, itemChild->value());
            if (err) return err;
        }
    }

    return std::nullopt;
}

} // namespace resource_manager
