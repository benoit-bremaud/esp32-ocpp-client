#pragma once

#include <functional>
#include <string>

namespace Core::Domain {

class IHardwareController {
public:
    virtual ~IHardwareController() = default;
    virtual bool enableCharging(int connectorId) = 0;
    virtual bool disableCharging(int connectorId) = 0;
    virtual float getCurrentMeterValue(int connectorId) = 0;
    virtual bool unlockConnector(int connectorId) = 0;
    virtual bool isConnectorPlugged(int connectorId) = 0;
    virtual std::string readRFIDTag() = 0;
    virtual void setStatusLED(int connectorId, const std::string& status) = 0;
    virtual bool isEmergencyStopPressed() = 0;

    virtual void setConnectorCallback(std::function<void(int, bool)> callback) = 0;
    virtual void setRFIDCallback(std::function<void(const std::string&)> callback) = 0;
    virtual void setEmergencyCallback(std::function<void()> callback) = 0;
};

} // namespace Core::Domain
