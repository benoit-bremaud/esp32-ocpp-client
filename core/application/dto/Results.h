#pragma once

#include <string>
#include <vector>

namespace Core::Application {

template<typename T>
struct CommandResult {
    bool success = false;
    std::string errorMessage;
    T data{};

    CommandResult() = default;
    explicit CommandResult(bool ok) : success(ok) {}

    static CommandResult<T> Success(const T& result) {
        CommandResult<T> cmd(true);
        cmd.data = result;
        return cmd;
    }

    static CommandResult<T> Failure(const std::string& error) {
        CommandResult<T> cmd(false);
        cmd.errorMessage = error;
        return cmd;
    }
};

struct TransactionResult {
    int transactionId = 0;
    std::string status;
    std::string idTagInfo;
    std::string timestamp;
};

struct AuthorizationResult {
    std::string status = "Unknown";
    std::string parentIdTag;
    std::string expiryDate;
    bool authorized = false;

    AuthorizationResult() = default;
    AuthorizationResult(const std::string& st, bool auth) : status(st), authorized(auth) {}
};

struct ConnectorStatus {
    int connectorId = 0;
    std::string status = "Unknown";
    std::string errorCode = "NoError";
    std::string info;
    std::string timestamp;
    bool available = false;
    bool charging = false;
    int currentTransactionId = 0;

    ConnectorStatus() = default;
    explicit ConnectorStatus(int id) : connectorId(id), status("Available"), available(true), charging(false) {}
};

struct SystemConfiguration {
    std::string centralSystemUrl;
    std::string chargePointId;
    int heartbeatInterval = 300;
    int meterValueSampleInterval = 60;
    int numberOfConnectors = 1;
    int securityProfile = 1;
    bool authorizeRemoteTxRequests = true;
    bool localAuthorizeOffline = true;
};

struct HardwareStatusData {
    bool emergencyStop = false;
};

struct NetworkStatus {
    bool wifiConnected = false;
    std::string wifiSSID;
    std::string ipAddress;
    bool ocppConnected = false;
    std::string centralSystemUrl;
    int signalStrength = 0;
    unsigned long connectionUptime = 0;
};

struct SystemStatus {
    std::string chargePointId;
    std::string firmwareVersion;
    std::string uptime;
    NetworkStatus network;
    HardwareStatusData hardware;
    std::vector<ConnectorStatus> connectors;
    SystemConfiguration configuration;
    unsigned long totalEnergyDelivered = 0;
    int totalTransactions = 0;
};

} // namespace Core::Application
