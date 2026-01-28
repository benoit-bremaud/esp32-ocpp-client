#pragma once

#include <functional>
#include <string>

namespace Core::Domain {

class IWebSocketClient {
public:
    virtual ~IWebSocketClient() = default;
    virtual bool connect(const std::string& url) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() = 0;
    virtual bool sendMessage(const std::string& message) = 0;
    virtual void setMessageCallback(std::function<void(const std::string&)> callback) = 0;
    virtual void setConnectionCallback(std::function<void(bool)> callback) = 0;
};

} // namespace Core::Domain
