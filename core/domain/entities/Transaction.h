#pragma once

#include <chrono>
#include <string>

namespace Core::Domain {

struct Transaction {
    int transactionId = 0;
    int connectorId = 0;
    std::string idTag;
    std::chrono::system_clock::time_point startTime{};
    std::chrono::system_clock::time_point stopTime{};
    int startMeterValue = 0;
    int stopMeterValue = 0;
    std::string stopReason;
    bool isActive = false;

    Transaction() = default;

    Transaction(int connector, const std::string& tag)
        : connectorId(connector), idTag(tag), startTime(std::chrono::system_clock::now()), isActive(true) {}
};

} // namespace Core::Domain
