#pragma once

#include <string>
#include <chrono>
#include <unordered_map>
#include <optional>

namespace resource_manager {

struct CacheEntry {
    int pid;
    std::string name;
    std::chrono::steady_clock::time_point timestamp;
};

class DiscoveryCache {
public:
    explicit DiscoveryCache(std::chrono::milliseconds ttl = std::chrono::seconds(5));

    void store(int pid, const std::string& name);
    std::optional<CacheEntry> lookup(int pid);
    void refresh(int pid);
    void evict(int pid);
    void clear();
    std::size_t size() const;

private:
    bool isExpired(const CacheEntry& entry) const;

    std::chrono::milliseconds ttl_;
    std::unordered_map<int, CacheEntry> cache_;
};

} // namespace resource_manager
