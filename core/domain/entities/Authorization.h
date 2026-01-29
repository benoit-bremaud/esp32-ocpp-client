#pragma once

#include <chrono>
#include <string>

namespace Core::Domain {

struct AuthorizationCacheEntry {
    std::string idTag;
    std::string status; // Accepted, Blocked, Expired, Invalid, ConcurrentTx
    std::chrono::system_clock::time_point expiryDate{};
    std::string parentIdTag;
};

} // namespace Core::Domain
