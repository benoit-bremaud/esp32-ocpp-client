#pragma once

#include <functional>
#include <string>

namespace Core::Domain {

class IWiFiManager {
public:
    virtual ~IWiFiManager() = default;
    virtual bool initialize() = 0;
    virtual bool connectToNetwork(const std::string& ssid, const std::string& password) = 0;
    virtual bool startConfigPortal() = 0;
    virtual bool isConnected() = 0;
    virtual std::string getIPAddress() = 0;
    virtual void setConnectionCallback(std::function<void(bool)> callback) = 0;
};

} // namespace Core::Domain
