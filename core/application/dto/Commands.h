#pragma once

#include <string>
#include <vector>

namespace Core::Application {

struct StartTransactionCommand {
    std::string idTag;
    int connectorId = 0;
    std::string timestamp;
    int meterStart = 0;
    std::string reservationId;

    StartTransactionCommand() = default;
    StartTransactionCommand(const std::string& tag, int connector, int meter)
        : idTag(tag), connectorId(connector), meterStart(meter) {}
};

struct StopTransactionCommand {
    int transactionId = 0;
    std::string idTag;
    std::string timestamp;
    int meterStop = 0;
    std::string reason = "Local";

    StopTransactionCommand() = default;
    StopTransactionCommand(int txId, const std::string& tag, int meter)
        : transactionId(txId), idTag(tag), meterStop(meter) {}
};

struct AuthorizeCommand {
    std::string idTag;
    std::string timestamp;

    explicit AuthorizeCommand(const std::string& tag) : idTag(tag) {}
};

struct StatusNotificationCommand {
    int connectorId = 0;
    std::string status;
    std::string errorCode = "NoError";
    std::string info;
    std::string timestamp;
    std::string vendorId;
    std::string vendorErrorCode;

    StatusNotificationCommand() = default;
    StatusNotificationCommand(int connector, const std::string& st)
        : connectorId(connector), status(st) {}
};

struct MeterValueData {
    std::string value = "0";
    std::string context = "Sample.Periodic";
    std::string format = "Raw";
    std::string measurand = "Energy.Active.Import.Register";
    std::string phase;
    std::string location = "Outlet";
    std::string unit = "Wh";

    explicit MeterValueData(const std::string& val = "0") : value(val) {}
};

struct MeterValuesCommand {
    int connectorId = 0;
    int transactionId = 0;
    std::string timestamp;
    std::vector<MeterValueData> meterValues;

    explicit MeterValuesCommand(int connector) : connectorId(connector) {}
};

struct ChangeConfigurationCommand {
    std::string key;
    std::string value;

    ChangeConfigurationCommand() = default;
    ChangeConfigurationCommand(const std::string& k, const std::string& v) : key(k), value(v) {}
};

struct RemoteStartTransactionCommand {
    int connectorId = 0;
    std::string idTag;
    int chargingProfileId = 0;
    std::string chargingProfileJson;

    RemoteStartTransactionCommand() = default;
    RemoteStartTransactionCommand(int connector, const std::string& tag)
        : connectorId(connector), idTag(tag) {}
};

struct RemoteStopTransactionCommand {
    int transactionId = 0;

    explicit RemoteStopTransactionCommand(int txId) : transactionId(txId) {}
};

} // namespace Core::Application
