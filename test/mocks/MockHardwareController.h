#pragma once

#include "../../core/domain/ports/IHardwareController.h"
#include <functional>
#include <map>
#include <string>

namespace MockInfrastructure {

class MockHardwareController : public Core::Domain::IHardwareController {
private:
    std::map<int, bool> chargingEnabled;
    std::map<int, bool> connectorPlugged;
    std::map<int, float> currentReadings;
    std::map<int, std::string> ledStatus;
    bool emergencyStopPressed = false;
    std::string lastRfidTag;

    std::function<void(int, bool)> connectorCallback;
    std::function<void(const std::string&)> rfidCallback;
    std::function<void()> emergencyCallback;

public:
    bool enableCharging(int connectorId) override {
        chargingEnabled[connectorId] = true;
        return true;
    }

    bool disableCharging(int connectorId) override {
        chargingEnabled[connectorId] = false;
        return true;
    }

    float getCurrentMeterValue(int connectorId) override {
        auto it = currentReadings.find(connectorId);
        return (it != currentReadings.end()) ? it->second : 0.0f;
    }

    bool unlockConnector(int connectorId) override {
        (void)connectorId;
        return true;
    }

    bool isConnectorPlugged(int connectorId) override {
        auto it = connectorPlugged.find(connectorId);
        return (it != connectorPlugged.end()) ? it->second : false;
    }

    std::string readRFIDTag() override {
        return lastRfidTag;
    }

    void setStatusLED(int connectorId, const std::string& status) override {
        ledStatus[connectorId] = status;
    }

    bool isEmergencyStopPressed() override {
        return emergencyStopPressed;
    }

    void setConnectorCallback(std::function<void(int, bool)> callback) override {
        connectorCallback = callback;
    }

    void setRFIDCallback(std::function<void(const std::string&)> callback) override {
        rfidCallback = callback;
    }

    void setEmergencyCallback(std::function<void()> callback) override {
        emergencyCallback = callback;
    }

    // Test helpers
    void setConnectorPlugged(int connectorId, bool plugged) {
        connectorPlugged[connectorId] = plugged;
        if (connectorCallback) {
            connectorCallback(connectorId, plugged);
        }
    }

    void setCurrentReading(int connectorId, float current) {
        currentReadings[connectorId] = current;
    }

    void setEmergencyStop(bool pressed) {
        emergencyStopPressed = pressed;
        if (emergencyCallback) {
            emergencyCallback();
        }
    }

    void simulateRfidTag(const std::string& tag) {
        lastRfidTag = tag;
        if (rfidCallback) {
            rfidCallback(tag);
        }
    }

    bool isChargingEnabled(int connectorId) const {
        auto it = chargingEnabled.find(connectorId);
        return (it != chargingEnabled.end()) ? it->second : false;
    }

    std::string getLedStatus(int connectorId) const {
        auto it = ledStatus.find(connectorId);
        return (it != ledStatus.end()) ? it->second : "";
    }
};

} // namespace MockInfrastructure
