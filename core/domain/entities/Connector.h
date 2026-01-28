#pragma once

#include <chrono>
#include <string>

namespace Core::Domain {

struct Connector {
    enum class Status {
        Available,
        Preparing,
        Charging,
        SuspendedEV,
        SuspendedEVSE,
        Finishing,
        Reserved,
        Unavailable,
        Faulted
    };

    int connectorId = 0;
    Status status = Status::Available;
    std::string errorCode;
    std::string info;
    std::chrono::system_clock::time_point timestamp{};
    std::string vendorId;
    std::string vendorErrorCode;

    Connector() = default;
    explicit Connector(int id)
        : connectorId(id), status(Status::Available), timestamp(std::chrono::system_clock::now()) {}
};

} // namespace Core::Domain
