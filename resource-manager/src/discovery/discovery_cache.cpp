#include "resource_manager/discovery/discovery_cache.h"

namespace resource_manager {

DiscoveryCache::DiscoveryCache(std::chrono::milliseconds ttl)
    : ttl_(ttl)
{
}

void DiscoveryCache::store(int pid, const std::string& name) {
    cache_[pid] = CacheEntry{
        pid,
        name,
        std::chrono::steady_clock::now()
    };
}

std::optional<CacheEntry> DiscoveryCache::lookup(int pid) {
    auto it = cache_.find(pid);
    if (it == cache_.end()) {
        return std::nullopt;
    }

    if (isExpired(it->second)) {
        cache_.erase(it);
        return std::nullopt;
    }

    return it->second;
}

void DiscoveryCache::refresh(int pid) {
    auto it = cache_.find(pid);
    if (it != cache_.end()) {
        it->second.timestamp = std::chrono::steady_clock::now();
    }
}

void DiscoveryCache::evict(int pid) {
    cache_.erase(pid);
}

void DiscoveryCache::clear() {
    cache_.clear();
}

std::size_t DiscoveryCache::size() const {
    return cache_.size();
}

bool DiscoveryCache::isExpired(const CacheEntry& entry) const {
    auto age = std::chrono::steady_clock::now() - entry.timestamp;
    return age >= ttl_;
}

} // namespace resource_manager
