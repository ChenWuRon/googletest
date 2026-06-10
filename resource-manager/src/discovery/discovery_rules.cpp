#include "resource_manager/discovery/discovery_rules.h"

#include <algorithm>
#include <cctype>

namespace resource_manager {

MatchType DiscoveryRules::parseType(const std::string& type) {
    std::string lower = type;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "prefix") return MatchType::Prefix;
    if (lower == "wildcard") return MatchType::Wildcard;
    return MatchType::Exact;
}

bool DiscoveryRules::match(const std::string& pattern, MatchType type, const std::string& value) {
    switch (type) {
        case MatchType::Exact:
            return matchExact(pattern, value);
        case MatchType::Prefix:
            return matchPrefix(pattern, value);
        case MatchType::Wildcard:
            return matchWildcard(pattern, value);
    }
    return false;
}

bool DiscoveryRules::matchExact(const std::string& pattern, const std::string& value) {
    return pattern == value;
}

bool DiscoveryRules::matchPrefix(const std::string& pattern, const std::string& value) {
    if (pattern.size() > value.size()) {
        return false;
    }
    return value.compare(0, pattern.size(), pattern) == 0;
}

bool DiscoveryRules::matchWildcard(const std::string& pattern, const std::string& value) {
    std::size_t pi = 0;
    std::size_t vi = 0;
    std::size_t starPos = std::string::npos;
    std::size_t matchPos = 0;

    while (vi < value.size()) {
        if (pi < pattern.size() && (pattern[pi] == value[vi] || pattern[pi] == '?')) {
            pi++;
            vi++;
        } else if (pi < pattern.size() && pattern[pi] == '*') {
            starPos = pi;
            matchPos = vi;
            pi++;
        } else if (starPos != std::string::npos) {
            pi = starPos + 1;
            matchPos++;
            vi = matchPos;
        } else {
            return false;
        }
    }

    while (pi < pattern.size() && pattern[pi] == '*') {
        pi++;
    }

    return pi == pattern.size();
}

} // namespace resource_manager
