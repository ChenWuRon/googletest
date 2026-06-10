#include "resource_manager/mode/mode.h"

namespace resource_manager {

bool operator==(const Mode& lhs, const Mode& rhs) {
    return lhs.service == rhs.service &&
           lhs.ns == rhs.ns &&
           lhs.entity == rhs.entity;
}

bool operator!=(const Mode& lhs, const Mode& rhs) {
    return !(lhs == rhs);
}

} // namespace resource_manager
