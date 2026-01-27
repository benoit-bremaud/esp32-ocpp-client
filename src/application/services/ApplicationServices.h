#pragma once

#include "../usecases/OCPPUseCases.h"
#include "../dto/ApplicationDTO.h"
#include "../../infrastructure/ocpp/OCPPClient.h"
#include <memory>
#include <queue>
#include <functional>

namespace Application {
    
    /**
     * @brief Application Services orchestrate business operations and coordinate
     * between use cases and infrastructure components.
     * 
     * Following CLEAN Architecture principles:
     * - Orchestrates complex business workflows
     * - Coordinates between multiple use cases
     * - Handles application-level concerns (events, notifications, etc.)
     * - Maintains separation between domain logic and infrastructure
     */
    
    /**
     * @brief Main charging station application service
     */
    class ChargingStationService {
    private:
        // Use case dependencies
        std::unique_ptr<UseCaseFactory> useCaseFactory;
        
        // Infrastructure dependencies
        Infrastructure::OCPPClient* ocppClient;
        Domain::IHardwareController* hardware;
        Domain::IWiFiManager* wifiManager;
        
        // Event handling
        std::queue<SystemEvent> eventQueue;
        std::vector<std::function<void(const SystemEvent&)>> eventSubscribers;
        
        // Internal state
        bool initialized = false;
        bool emergencyStopActive = false;
        std::map<int, std::string> connectorStatuses;
        
        // Event processing
        void processEvents();
        void publishEvent(const SystemEvent& event);
        
        // Hardware event handlers
        void onConnectorPlugged(int connectorId);
        void onConnectorUnplugged(int connectorId);
        void onRFIDTagDetected(const std::string& idTag);
        void onEmergencyStopActivated();
        void onEmergencyStopCleared();
        
        // Network event handlers
        void onNetworkConnected();
        void onNetworkDisconnected();
        void onOCPPConnected();
        void onOCPPDisconnected();
        
        // Transaction helpers
        bool canStartTransaction(int connectorId);
        void updateConnectorStatus(int connectorId, const std::string& status);
        
    public:
        ChargingStationService(
            std::unique_ptr<UseCaseFactory> factory,
            Infrastructure::OCPPClient* ocpp,
            Domain::IHardwareController* hw,
            Domain::IWiFiManager* wifi
        );
        
        ~ChargingStationService();
        
        // Lifecycle management
        bool initialize();
        void shutdown();
        void loop(); // Must be called regularly in main loop
        
        // Transaction management
        CommandResult<TransactionResult> startTransaction(const std::string& idTag, int connectorId);
        CommandResult<TransactionResult> stopTransaction(int transactionId, const std::string& reason = "Local");
        CommandResult<AuthorizationResult> authorize(const std::string& idTag);
        
        // Remote operations
        CommandResult<TransactionResult> remoteStartTransaction(int connectorId, const std::string& idTag);
        CommandResult<TransactionResult> remoteStopTransaction(int transactionId);
        
        // Status and configuration
        SystemStatus getSystemStatus();
        CommandResult<bool> changeConfiguration(const std::string& key, const std::string& value);
        std::vector<ConnectorStatus> getConnectorStatuses();
        
        // Event subscription
        void subscribeToEvents(std::function<void(const SystemEvent&)> callback);
        void unsubscribeFromEvents();
        
        // Emergency operations
        void handleEmergencyStop();
        void clearEmergencyStop();
        bool isEmergencyStopActive() const { return emergencyStopActive; }
        
        // Hardware operations
        bool testConnector(int connectorId);
        bool calibrateConnector(int connectorId);
        void updateAllConnectorStatuses();
        
        // Diagnostics and maintenance
        void performSelfTest();
        std::vector<std::string> getDiagnosticInfo();
        void resetSystem();
        
        // Statistics
        struct Statistics {
            unsigned long totalTransactions = 0;
            unsigned long totalEnergyDelivered = 0;
            unsigned long totalUptime = 0;
            unsigned long totalErrors = 0;
            std::map<int, unsigned long> connectorUsage;
        };
        
        Statistics getStatistics() const;
    };
    
    /**
     * @brief Configuration management service
     */
    class ConfigurationService {
    private:
        Domain::IConfigRepository* configRepo;
        ChargingStationService* chargingService;
        
        // Configuration validation
        bool validateConfiguration(const SystemConfiguration& config);
        bool requiresRestart(const std::string& key);
        
    public:
        ConfigurationService(Domain::IConfigRepository* repo, ChargingStationService* service)
            : configRepo(repo), chargingService(service) {}
        
        // Configuration management
        SystemConfiguration getCurrentConfiguration();
        CommandResult<bool> updateConfiguration(const SystemConfiguration& config);
        CommandResult<bool> updateConfigurationKey(const std::string& key, const std::string& value);
        
        // Configuration validation and defaults
        SystemConfiguration getDefaultConfiguration();
        std::vector<std::string> getConfigurationKeys();
        std::map<std::string, std::string> getAllConfigurationValues();
        
        // Factory reset
        void resetToDefaults();
        bool backupConfiguration(const std::string& backupPath);
        bool restoreConfiguration(const std::string& backupPath);
    };
    
    /**
     * @brief Transaction management service
     */
    class TransactionService {
    private:
        Domain::ITransactionRepository* transactionRepo;
        Domain::IHardwareController* hardware;
        ChargingStationService* chargingService;
        
        // Transaction validation
        bool validateTransactionStart(int connectorId, const std::string& idTag);
        bool validateTransactionStop(int transactionId);
        
        // Meter value collection
        void collectMeterValues(int transactionId);
        std::chrono::steady_clock::time_point lastMeterValueCollection;
        
    public:
        TransactionService(
            Domain::ITransactionRepository* repo,
            Domain::IHardwareController* hw,
            ChargingStationService* service
        ) : transactionRepo(repo), hardware(hw), chargingService(service) {}
        
        // Transaction lifecycle
        CommandResult<TransactionResult> createTransaction(const std::string& idTag, int connectorId);
        CommandResult<TransactionResult> endTransaction(int transactionId, const std::string& reason);
        
        // Transaction queries
        std::vector<Domain::Transaction> getActiveTransactions();
        std::optional<Domain::Transaction> getTransactionById(int transactionId);
        std::vector<Domain::Transaction> getTransactionHistory(int limit = 100);
        
        // Transaction monitoring
        void startMeterValueCollection(int transactionId);
        void stopMeterValueCollection(int transactionId);
        void collectPeriodicMeterValues();
        
        // Transaction cleanup
        void cleanupOldTransactions(int maxAge = 30); // days
        void archiveCompletedTransactions();
    };
    
    /**
     * @brief Security and authorization service
     */
    class SecurityService {
    private:
        Domain::IConfigRepository* configRepo;
        Infrastructure::ICertificateManager* certManager;
        
        // Authorization cache
        struct AuthCacheEntry {
            std::string idTag;
            std::string status;
            std::chrono::system_clock::time_point expiry;
            std::string parentIdTag;
        };
        
        std::map<std::string, AuthCacheEntry> authCache;
        
        // Authorization helpers
        bool isIdTagInLocalList(const std::string& idTag);
        void updateAuthorizationCache(const std::string& idTag, const AuthorizationResult& result);
        
    public:
        SecurityService(Domain::IConfigRepository* repo, Infrastructure::ICertificateManager* cert)
            : configRepo(repo), certManager(cert) {}
        
        // Authorization
        CommandResult<AuthorizationResult> authorizeIdTag(const std::string& idTag);
        void updateLocalAuthorizationList(const std::vector<std::string>& authorizedTags);
        void clearAuthorizationCache();
        
        // Security profile management
        bool updateSecurityProfile(int profile);
        int getCurrentSecurityProfile();
        bool installCertificate(Infrastructure::CertificateType type, const std::string& certificate);
        
        // Access control
        bool hasAdminAccess(const std::string& idTag);
        bool hasMaintenanceAccess(const std::string& idTag);
        std::vector<std::string> getAuthorizedTags();
    };
    
    /**
     * @brief Main application facade that coordinates all services
     */
    class ChargingStationApplication {
    private:
        // Core services
        std::unique_ptr<ChargingStationService> chargingService;
        std::unique_ptr<ConfigurationService> configService;
        std::unique_ptr<TransactionService> transactionService;
        std::unique_ptr<SecurityService> securityService;
        
        // Infrastructure components
        Infrastructure::OCPPClient* ocppClient;
        Domain::IHardwareController* hardware;
        Domain::IWiFiManager* wifiManager;
        Infrastructure::ICertificateManager* certManager;
        
        // Application state
        bool initialized = false;
        std::chrono::steady_clock::time_point startTime;
        
    public:
        ChargingStationApplication(
            Infrastructure::OCPPClient* ocpp,
            Domain::IHardwareController* hw,
            Domain::IWiFiManager* wifi,
            Infrastructure::ICertificateManager* cert,
            std::unique_ptr<UseCaseFactory> factory,
            Domain::IConfigRepository* configRepo,
            Domain::ITransactionRepository* transactionRepo
        );
        
        ~ChargingStationApplication();
        
        // Lifecycle
        bool initialize();
        void shutdown();
        void loop(); // Main application loop
        
        // Service access
        ChargingStationService* getChargingService() { return chargingService.get(); }
        ConfigurationService* getConfigService() { return configService.get(); }
        TransactionService* getTransactionService() { return transactionService.get(); }
        SecurityService* getSecurityService() { return securityService.get(); }
        
        // Application-level operations
        SystemStatus getOverallStatus();
        bool performStartupSelfTest();
        void handleCriticalError(const std::string& error);
        
        // Factory method
        static std::unique_ptr<ChargingStationApplication> create(
            Infrastructure::OCPPClient* ocpp,
            Domain::IHardwareController* hw,
            Domain::IWiFiManager* wifi,
            Infrastructure::ICertificateManager* cert,
            Domain::IConfigRepository* configRepo,
            Domain::ITransactionRepository* transactionRepo
        );
    };
}