#pragma once

#include <string>
#include <vector>

namespace resource_manager {

enum class MatchType {
    Exact,
    Prefix,
    Wildcard,
};

class DiscoveryRules {
public:
    static MatchType parseType(const std::string& type);

    static bool match(const std::string& pattern, MatchType type, const std::string& value);

    static bool matchExact(const std::string& pattern, const std::string& value);
    static bool matchPrefix(const std::string& pattern, const std::string& value);
    static bool matchWildcard(const std::string& pattern, const std::string& value);
};

} // namespace resource_manager
