#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>
#include <chrono>

namespace resource_manager {

template <typename T>
using Result = std::optional<T>;

struct Error;
struct ProcessInfo;
struct ThreadInfo;

} // namespace resource_manager
