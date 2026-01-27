#pragma once

#include <string>
#include <functional>
#include <ArduinoJson.h>

namespace Domain {
    
    // Forward declarations
    class Transaction;
    class Configuration;
    
    /**
     * @brief Repository interface for managing OCPP configuration
     */
    class IConfigRepository {
    public:
        virtual ~IConfigRepository() = default;
        virtual bool saveConfiguration(const Configuration& config) = 0;
        virtual Configuration loadConfiguration() = 0;
        virtual bool hasConfiguration() = 0;
        virtual void resetToDefaults() = 0;
    };
    
    /**
     * @brief Repository interface for managing charging transactions
     */
    class ITransactionRepository {
    public:
        virtual ~ITransactionRepository() = default;
        virtual bool saveTransaction(const Transaction& transaction) = 0;
        virtual std::optional<Transaction> getTransaction(int transactionId) = 0;
        virtual std::vector<Transaction> getActiveTransactions() = 0;
        virtual bool removeTransaction(int transactionId) = 0;
        virtual void clearAllTransactions() = 0;
    };
    
    /**
     * @brief Hardware abstraction interface
     */
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
    };
    
    /**
     * @brief WebSocket communication interface
     */
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
    
    /**
     * @brief File system interface for persistence
     */
    class IFileSystem {
    public:
        virtual ~IFileSystem() = default;
        virtual bool writeFile(const std::string& path, const std::string& content) = 0;
        virtual std::string readFile(const std::string& path) = 0;
        virtual bool fileExists(const std::string& path) = 0;
        virtual bool deleteFile(const std::string& path) = 0;
        virtual bool initialize() = 0;
    };
    
    /**
     * @brief WiFi management interface
     */
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
}