#include "resource_manager/driver/resource_snapshot.h"

#include <sstream>

namespace resource_manager {

ResourceSnapshot ResourceSnapshot::capture(ICgroupDriver& driver, const ConfigDomain& domain) {
    ResourceSnapshot snapshot;
    snapshot.capturedAt_ = std::chrono::system_clock::now();

    for (const auto& [name, child] : domain.root().children()) {
        if (child->type() != ConfigNodeType::GROUP) continue;

        const std::string& cgroupPath = child->name();

        for (const auto& [ctrlName, ctrlChild] : child->children()) {
            if (ctrlChild->type() != ConfigNodeType::CONTROLLER) continue;

            for (const auto& [itemName, itemChild] : ctrlChild->children()) {
                if (itemChild->type() != ConfigNodeType::ITEM) continue;

                std::string value;
                auto result = driver.getValue(cgroupPath, itemName);
                if (result) {
                    value = std::move(*result);
                }

                snapshot.entries_.push_back({
                    cgroupPath, ctrlName, itemName, std::move(value)
                });
            }
        }
    }

    return snapshot;
}

ResourceDiff ResourceSnapshot::compare(const ResourceSnapshot& other) const {
    ResourceDiff diff;
    diff.oldTimestamp = other.capturedAt_;
    diff.newTimestamp = capturedAt_;

    for (const auto& entry : entries_) {
        bool found = false;
        for (const auto& oldEntry : other.entries_) {
            if (oldEntry.cgroupPath == entry.cgroupPath &&
                oldEntry.controller == entry.controller &&
                oldEntry.item == entry.item)
            {
                found = true;
                if (oldEntry.value != entry.value) {
                    diff.changes.push_back({
                        entry.cgroupPath,
                        entry.controller,
                        entry.item,
                        oldEntry.value,
                        entry.value
                    });
                }
                break;
            }
        }
        if (!found) {
            diff.changes.push_back({
                entry.cgroupPath,
                entry.controller,
                entry.item,
                "",
                entry.value
            });
        }
    }

    return diff;
}

std::string ResourceSnapshot::serialize() const {
    std::ostringstream oss;
    auto t = std::chrono::system_clock::to_time_t(capturedAt_);
    oss << "# ResourceSnapshot captured at " << std::ctime(&t);

    std::string lastPath;
    for (const auto& entry : entries_) {
        if (entry.cgroupPath != lastPath) {
            oss << "\n[" << entry.cgroupPath << "]\n";
            lastPath = entry.cgroupPath;
        }
        oss << "  " << entry.controller << "." << entry.item
            << " = " << entry.value << "\n";
    }

    return oss.str();
}

const std::vector<ResourceEntry>& ResourceSnapshot::entries() const {
    return entries_;
}

const std::chrono::system_clock::time_point& ResourceSnapshot::capturedAt() const {
    return capturedAt_;
}

bool ResourceDiff::empty() const {
    return changes.empty();
}

std::size_t ResourceDiff::size() const {
    return changes.size();
}

} // namespace resource_manager
