#pragma once

#include <string>
#include <chrono>
#include <map>
#include <vector>
#include <optional>

namespace Application {
    
    /**
     * @brief Data Transfer Objects (DTOs) for Application Layer
     * 
     * These DTOs provide clean data transfer between layers without exposing
     * infrastructure concerns. Following SOLID principles by keeping data 
     * transfer separate from business logic.
     */
    
    /**
     * @brief Command for starting a charging transaction
     */
    struct StartTransactionCommand {
        std::string idTag;
        int connectorId;
        std::string timestamp;
        int meterStart;
        std::string reservationId = "";
        
        StartTransactionCommand(const std::string& tag, int connector, int meter)
            : idTag(tag), connectorId(connector), meterStart(meter) {}
    };
    
    /**
     * @brief Command for stopping a charging transaction
     */
    struct StopTransactionCommand {
        int transactionId;
        std::string idTag;
        std::string timestamp;
        int meterStop;
        std::string reason = "Local";
        
        StopTransactionCommand(int txId, const std::string& tag, int meter)
            : transactionId(txId), idTag(tag), meterStop(meter) {}
    };
    
    /**
     * @brief Command for authorization request
     */
    struct AuthorizeCommand {
        std::string idTag;
        std::string timestamp;
        
        AuthorizeCommand(const std::string& tag) : idTag(tag) {}
    };
    
    /**
     * @brief Command for status notification
     */
    struct StatusNotificationCommand {
        int connectorId;
        std::string status;
        std::string errorCode = "NoError";
        std::string info = "";
        std::string timestamp;
        std::string vendorId = "";
        std::string vendorErrorCode = "";
        
        StatusNotificationCommand(int connector, const std::string& st)
            : connectorId(connector), status(st) {}
    };
    
    /**
     * @brief Command for meter values reporting
     */
    struct MeterValuesCommand {
        int connectorId;
        int transactionId = 0;
        std::string timestamp;
        std::vector<struct MeterValueData> meterValues;
        
        struct MeterValueData {
            std::string value;
            std::string context = "Sample.Periodic";
            std::string format = "Raw";
            std::string measurand = "Energy.Active.Import.Register";
            std::string phase = "";
            std::string location = "Outlet";
            std::string unit = "Wh";
        };
        
        MeterValuesCommand(int connector) : connectorId(connector) {}
    };
    
    /**
     * @brief Command for configuration changes
     */
    struct ChangeConfigurationCommand {
        std::string key;
        std::string value;
        
        ChangeConfigurationCommand(const std::string& k, const std::string& v)
            : key(k), value(v) {}
    };
    
    /**
     * @brief Command for remote start transaction
     */
    struct RemoteStartTransactionCommand {
        int connectorId;
        std::string idTag;
        int chargingProfileId = 0;
        std::string chargingProfileJson = "";
        
        RemoteStartTransactionCommand(int connector, const std::string& tag)
            : connectorId(connector), idTag(tag) {}
    };
    
    /**
     * @brief Command for remote stop transaction
     */
    struct RemoteStopTransactionCommand {
        int transactionId;
        
        RemoteStopTransactionCommand(int txId) : transactionId(txId) {}
    };
    
    /**
     * @brief Result object for command execution
     */
    template<typename T>
    struct CommandResult {
        bool success;
        std::string errorMessage;
        T data;
        
        CommandResult(bool success = false) : success(success) {}
        
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
    
    /**
     * @brief Transaction result data
     */
    struct TransactionResult {
        int transactionId;
        std::string status;
        std::string idTagInfo;
        std::string timestamp;
    };
    
    /**
     * @brief Authorization result data
     */
    struct AuthorizationResult {
        std::string status;
        std::string parentIdTag = "";
        std::string expiryDate = "";
        bool authorized;
        
        AuthorizationResult(const std::string& st, bool auth) 
            : status(st), authorized(auth) {}
    };
    
    /**
     * @brief Connector status data
     */
    struct ConnectorStatus {
        int connectorId;
        std::string status;
        std::string errorCode;
        std::string info;
        std::string timestamp;
        bool available;
        bool charging;
        int currentTransactionId = 0;
        
        ConnectorStatus(int id) : connectorId(id), available(true), charging(false) {}
    };
    
    /**
     * @brief System configuration data
     */
    struct SystemConfiguration {
        std::string centralSystemUrl;
        std::string chargePointId;
        int heartbeatInterval;
        int meterValueSampleInterval;
        int numberOfConnectors;
        int securityProfile;
        bool authorizeRemoteTxRequests;
        bool localAuthorizeOffline;
        
        SystemConfiguration() {
            heartbeatInterval = 300;
            meterValueSampleInterval = 60;
            numberOfConnectors = 1;
            securityProfile = 1;
            authorizeRemoteTxRequests = true;
            localAuthorizeOffline = true;
        }
    };
    
    /**
     * @brief Hardware status data
     */
    struct HardwareStatusData {
        bool emergencyStop;
        std::map<int, bool> relayStates;
        std::map<int, bool> connectorStates;
        std::map<int, float> currentReadings;
        std::map<int, float> powerReadings;
        bool rfidReaderConnected;
        std::string lastRfidTag;
        
        HardwareStatusData() {
            emergencyStop = false;
            rfidReaderConnected = false;
        }
    };
    
    /**
     * @brief Network connection status
     */
    struct NetworkStatus {
        bool wifiConnected;
        std::string wifiSSID;
        std::string ipAddress;
        bool ocppConnected;
        std::string centralSystemUrl;
        int signalStrength;
        unsigned long connectionUptime;
        
        NetworkStatus() {
            wifiConnected = false;
            ocppConnected = false;
            signalStrength = 0;
            connectionUptime = 0;
        }
    };
    
    /**
     * @brief Complete system status aggregation
     */
    struct SystemStatus {
        std::string chargePointId;
        std::string firmwareVersion;
        std::string uptime;
        NetworkStatus network;
        HardwareStatusData hardware;
        std::vector<ConnectorStatus> connectors;
        SystemConfiguration configuration;
        unsigned long totalEnergyDelivered;
        int totalTransactions;
        
        SystemStatus() {
            totalEnergyDelivered = 0;
            totalTransactions = 0;
        }
    };
    
    /**
     * @brief Event data for system events
     */
    struct SystemEvent {
        enum Type {
            CONNECTOR_PLUGGED,
            CONNECTOR_UNPLUGGED,
            RFID_TAG_DETECTED,
            TRANSACTION_STARTED,
            TRANSACTION_STOPPED,
            EMERGENCY_STOP_ACTIVATED,
            EMERGENCY_STOP_CLEARED,
            NETWORK_CONNECTED,
            NETWORK_DISCONNECTED,
            OCPP_CONNECTED,
            OCPP_DISCONNECTED,
            CONFIGURATION_CHANGED,
            FIRMWARE_UPDATE_AVAILABLE,
            ERROR_OCCURRED
        };
        
        Type type;
        std::string timestamp;
        std::string message;
        std::map<std::string, std::string> data;
        
        SystemEvent(Type t, const std::string& msg) : type(t), message(msg) {}
    };
}