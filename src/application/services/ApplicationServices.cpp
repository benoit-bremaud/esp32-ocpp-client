#include "ApplicationServices.h"
#include "../../domain/entities/OCPPEntities.h"
#include <Arduino.h>

using namespace Application;
using namespace Domain;

// =============================================================================
// ChargingStationService Implementation
// =============================================================================

ChargingStationService::ChargingStationService(
    std::unique_ptr<UseCaseFactory> factory,
    Infrastructure::OCPPClient* ocpp,
    Domain::IHardwareController* hw,
    Domain::IWiFiManager* wifi
) : useCaseFactory(std::move(factory)), ocppClient(ocpp), hardware(hw), wifiManager(wifi) {
}

ChargingStationService::~ChargingStationService() {
    shutdown();
}

bool ChargingStationService::initialize() {
    if (initialized) {
        return true;
    }
    
    Serial.println("Initializing Charging Station Service...");
    
    // Setup hardware event callbacks
    if (hardware) {
        hardware->setConnectorCallback([this](int connectorId, bool plugged) {
            if (plugged) {
                this->onConnectorPlugged(connectorId);
            } else {
                this->onConnectorUnplugged(connectorId);
            }
        });
        
        hardware->setRFIDCallback([this](const std::string& idTag) {
            this->onRFIDTagDetected(idTag);
        });
        
        hardware->setEmergencyCallback([this]() {
            this->onEmergencyStopActivated();
        });
    }
    
    // Setup WiFi event callbacks
    if (wifiManager) {
        wifiManager->setConnectionCallback([this](bool connected) {
            if (connected) {
                this->onNetworkConnected();
            } else {
                this->onNetworkDisconnected();
            }
        });
    }
    
    // Initialize connector statuses
    updateAllConnectorStatuses();
    
    initialized = true;
    Serial.println("Charging Station Service initialized successfully");
    
    // Publish initialization event
    SystemEvent initEvent(SystemEvent::Type::OCPP_CONNECTED, "Charging Station Service initialized");
    publishEvent(initEvent);
    
    return true;
}

void ChargingStationService::shutdown() {
    if (!initialized) {
        return;
    }
    
    Serial.println("Shutting down Charging Station Service...");
    
    // Stop all active transactions
    if (useCaseFactory) {
        auto statusUseCase = useCaseFactory->createGetSystemStatusUseCase();
        auto systemStatus = statusUseCase->execute();
        
        for (const auto& connector : systemStatus.connectors) {
            if (connector.charging && connector.currentTransactionId > 0) {
                stopTransaction(connector.currentTransactionId, "Shutdown");
            }
        }
    }
    
    // Clear event subscribers
    eventSubscribers.clear();
    
    // Clear event queue
    while (!eventQueue.empty()) {
        eventQueue.pop();
    }
    
    initialized = false;
    Serial.println("Charging Station Service shutdown complete");
}

void ChargingStationService::loop() {
    if (!initialized) {
        return;
    }
    
    // Process pending events
    processEvents();
    
    // Handle emergency stop state
    if (hardware && hardware->isEmergencyStopPressed() != emergencyStopActive) {
        if (hardware->isEmergencyStopPressed()) {
            handleEmergencyStop();
        } else {
            clearEmergencyStop();
        }
    }
}

void ChargingStationService::processEvents() {
    // Process up to 10 events per loop to avoid blocking
    int eventsProcessed = 0;
    while (!eventQueue.empty() && eventsProcessed < 10) {
        SystemEvent event = eventQueue.front();
        eventQueue.pop();
        
        // Notify all subscribers
        for (const auto& subscriber : eventSubscribers) {
            subscriber(event);
        }
        
        eventsProcessed++;
    }
}

void ChargingStationService::publishEvent(const SystemEvent& event) {
    eventQueue.push(event);
    
    // Log event
    Serial.printf("Event: %s - %s\n", 
                  std::to_string(static_cast<int>(event.type)).c_str(),
                  event.message.c_str());
}

CommandResult<TransactionResult> ChargingStationService::startTransaction(const std::string& idTag, int connectorId) {
    if (!initialized) {
        return CommandResult<TransactionResult>::Failure("Service not initialized");
    }
    
    if (emergencyStopActive) {
        return CommandResult<TransactionResult>::Failure("Emergency stop active");
    }
    
    if (!canStartTransaction(connectorId)) {
        return CommandResult<TransactionResult>::Failure("Cannot start transaction on connector");
    }
    
    Serial.printf("Starting transaction - Connector: %d, IdTag: %s\n", connectorId, idTag.c_str());
    
    // Get current meter value
    int meterStart = static_cast<int>(hardware->getCurrentMeterValue(connectorId) * 1000); // Convert to Wh
    
    // Create start transaction command
    StartTransactionCommand command(idTag, connectorId, meterStart);
    command.timestamp = ""; // Would be filled with actual timestamp
    
    // Execute use case
    auto useCase = useCaseFactory->createStartTransactionUseCase();
    auto result = useCase->execute(command);
    
    if (result.success) {
        // Update connector status
        updateConnectorStatus(connectorId, "Charging");
        
        // Publish event
        SystemEvent event(SystemEvent::Type::TRANSACTION_STARTED, "Transaction started");
        event.data["connectorId"] = std::to_string(connectorId);
        event.data["idTag"] = idTag;
        event.data["transactionId"] = std::to_string(result.data.transactionId);
        publishEvent(event);
        
        Serial.printf("Transaction started successfully - ID: %d\n", result.data.transactionId);
    }
    
    return result;
}

CommandResult<TransactionResult> ChargingStationService::stopTransaction(int transactionId, const std::string& reason) {
    if (!initialized) {
        return CommandResult<TransactionResult>::Failure("Service not initialized");
    }
    
    Serial.printf("Stopping transaction - ID: %d, Reason: %s\n", transactionId, reason.c_str());
    
    // Get transaction details first
    auto transactionRepo = useCaseFactory.get(); // This would need proper access to repo
    
    // Get current meter value (would need connector ID from transaction)
    int meterStop = 0; // This would be calculated properly
    
    // Create stop transaction command
    StopTransactionCommand command(transactionId, "", meterStop);
    command.reason = reason;
    command.timestamp = ""; // Would be filled with actual timestamp
    
    // Execute use case
    auto useCase = useCaseFactory->createStopTransactionUseCase();
    auto result = useCase->execute(command);
    
    if (result.success) {
        // Update connector status (would need connector ID)
        // updateConnectorStatus(connectorId, "Available");
        
        // Publish event
        SystemEvent event(SystemEvent::Type::TRANSACTION_STOPPED, "Transaction stopped");
        event.data["transactionId"] = std::to_string(transactionId);
        event.data["reason"] = reason;
        publishEvent(event);
        
        Serial.printf("Transaction stopped successfully - ID: %d\n", transactionId);
    }
    
    return result;
}

CommandResult<AuthorizationResult> ChargingStationService::authorize(const std::string& idTag) {
    if (!initialized) {
        return CommandResult<AuthorizationResult>::Failure("Service not initialized");
    }
    
    Serial.printf("Authorizing ID tag: %s\n", idTag.c_str());
    
    // Create authorization command
    AuthorizeCommand command(idTag);
    
    // Execute use case
    auto useCase = useCaseFactory->createAuthorizeUseCase();
    auto result = useCase->execute(command);
    
    if (result.success) {
        Serial.printf("Authorization successful - ID: %s, Status: %s\n", 
                     idTag.c_str(), result.data.status.c_str());
    }
    
    return result;
}

CommandResult<TransactionResult> ChargingStationService::remoteStartTransaction(int connectorId, const std::string& idTag) {
    if (!initialized) {
        return CommandResult<TransactionResult>::Failure("Service not initialized");
    }
    
    Serial.printf("Remote start transaction - Connector: %d, IdTag: %s\n", connectorId, idTag.c_str());
    
    // Create remote start command
    RemoteStartTransactionCommand command(connectorId, idTag);
    
    // Execute use case
    auto useCase = useCaseFactory->createRemoteStartTransactionUseCase();
    auto result = useCase->execute(command);
    
    if (result.success) {
        // Publish event
        SystemEvent event(SystemEvent::Type::TRANSACTION_STARTED, "Remote transaction started");
        event.data["connectorId"] = std::to_string(connectorId);
        event.data["idTag"] = idTag;
        event.data["transactionId"] = std::to_string(result.data.transactionId);
        publishEvent(event);
    }
    
    return result;
}

CommandResult<TransactionResult> ChargingStationService::remoteStopTransaction(int transactionId) {
    if (!initialized) {
        return CommandResult<TransactionResult>::Failure("Service not initialized");
    }
    
    Serial.printf("Remote stop transaction - ID: %d\n", transactionId);
    
    // Create remote stop command
    RemoteStopTransactionCommand command(transactionId);
    
    // Execute use case
    auto useCase = useCaseFactory->createRemoteStopTransactionUseCase();
    auto result = useCase->execute(command);
    
    if (result.success) {
        // Publish event
        SystemEvent event(SystemEvent::Type::TRANSACTION_STOPPED, "Remote transaction stopped");
        event.data["transactionId"] = std::to_string(transactionId);
        publishEvent(event);
    }
    
    return result;
}

SystemStatus ChargingStationService::getSystemStatus() {
    if (!initialized) {
        return SystemStatus();
    }
    
    auto useCase = useCaseFactory->createGetSystemStatusUseCase();
    return useCase->execute();
}

CommandResult<bool> ChargingStationService::changeConfiguration(const std::string& key, const std::string& value) {
    if (!initialized) {
        return CommandResult<bool>::Failure("Service not initialized");
    }
    
    Serial.printf("Changing configuration - Key: %s, Value: %s\n", key.c_str(), value.c_str());
    
    // Create configuration change command
    ChangeConfigurationCommand command(key, value);
    
    // Execute use case
    auto useCase = useCaseFactory->createChangeConfigurationUseCase();
    auto result = useCase->execute(command);
    
    if (result.success) {
        // Publish event
        SystemEvent event(SystemEvent::Type::CONFIGURATION_CHANGED, "Configuration changed");
        event.data["key"] = key;
        event.data["value"] = value;
        publishEvent(event);
    }
    
    return result;
}

void ChargingStationService::subscribeToEvents(std::function<void(const SystemEvent&)> callback) {
    eventSubscribers.push_back(callback);
}

void ChargingStationService::unsubscribeFromEvents() {
    eventSubscribers.clear();
}

bool ChargingStationService::canStartTransaction(int connectorId) {
    if (emergencyStopActive) {
        return false;
    }
    
    if (!hardware || !hardware->isConnectorPlugged(connectorId)) {
        return false;
    }
    
    // Check if connector already has active transaction
    auto status = getSystemStatus();
    for (const auto& connector : status.connectors) {
        if (connector.connectorId == connectorId && connector.charging) {
            return false;
        }
    }
    
    return true;
}

void ChargingStationService::updateConnectorStatus(int connectorId, const std::string& status) {
    connectorStatuses[connectorId] = status;
    
    if (hardware) {
        hardware->setStatusLED(connectorId, status);
    }
    
    // Send OCPP status notification
    if (ocppClient && ocppClient->isConnected()) {
        ocppClient->sendStatusNotification(connectorId, status);
    }
}

void ChargingStationService::updateAllConnectorStatuses() {
    if (!hardware) {
        return;
    }
    
    auto status = getSystemStatus();
    for (const auto& connector : status.connectors) {
        std::string currentStatus = connector.charging ? "Charging" : 
                                  (connector.available ? "Available" : "Unavailable");
        updateConnectorStatus(connector.connectorId, currentStatus);
    }
}

void ChargingStationService::onConnectorPlugged(int connectorId) {
    Serial.printf("Connector %d plugged\n", connectorId);
    
    updateConnectorStatus(connectorId, "Available");
    
    SystemEvent event(SystemEvent::Type::CONNECTOR_PLUGGED, "Connector plugged");
    event.data["connectorId"] = std::to_string(connectorId);
    publishEvent(event);
}

void ChargingStationService::onConnectorUnplugged(int connectorId) {
    Serial.printf("Connector %d unplugged\n", connectorId);
    
    // Stop any active transaction on this connector
    auto status = getSystemStatus();
    for (const auto& connector : status.connectors) {
        if (connector.connectorId == connectorId && connector.charging) {
            stopTransaction(connector.currentTransactionId, "EvDisconnected");
            break;
        }
    }
    
    updateConnectorStatus(connectorId, "Unavailable");
    
    SystemEvent event(SystemEvent::Type::CONNECTOR_UNPLUGGED, "Connector unplugged");
    event.data["connectorId"] = std::to_string(connectorId);
    publishEvent(event);
}

void ChargingStationService::onRFIDTagDetected(const std::string& idTag) {
    Serial.printf("RFID tag detected: %s\n", idTag.c_str());
    
    SystemEvent event(SystemEvent::Type::RFID_TAG_DETECTED, "RFID tag detected");
    event.data["idTag"] = idTag;
    publishEvent(event);
    
    // Authorize the tag
    auto authResult = authorize(idTag);
    
    if (authResult.success && authResult.data.authorized) {
        // Find available connector and start transaction
        auto status = getSystemStatus();
        for (const auto& connector : status.connectors) {
            if (connector.available && !connector.charging) {
                startTransaction(idTag, connector.connectorId);
                break;
            }
        }
    } else {
        Serial.printf("RFID tag %s not authorized: %s\n", 
                     idTag.c_str(), authResult.errorMessage.c_str());
    }
}

void ChargingStationService::onEmergencyStopActivated() {
    handleEmergencyStop();
}

void ChargingStationService::onEmergencyStopCleared() {
    clearEmergencyStop();
}

void ChargingStationService::handleEmergencyStop() {
    if (emergencyStopActive) {
        return;
    }
    
    Serial.println("EMERGENCY STOP ACTIVATED!");
    emergencyStopActive = true;
    
    // Stop all active transactions immediately
    auto status = getSystemStatus();
    for (const auto& connector : status.connectors) {
        if (connector.charging && connector.currentTransactionId > 0) {
            stopTransaction(connector.currentTransactionId, "EmergencyStop");
        }
        updateConnectorStatus(connector.connectorId, "Faulted");
    }
    
    SystemEvent event(SystemEvent::Type::EMERGENCY_STOP_ACTIVATED, "Emergency stop activated");
    publishEvent(event);
}

void ChargingStationService::clearEmergencyStop() {
    if (!emergencyStopActive) {
        return;
    }
    
    Serial.println("Emergency stop cleared");
    emergencyStopActive = false;
    
    // Update all connector statuses
    updateAllConnectorStatuses();
    
    SystemEvent event(SystemEvent::Type::EMERGENCY_STOP_CLEARED, "Emergency stop cleared");
    publishEvent(event);
}

void ChargingStationService::onNetworkConnected() {
    Serial.println("Network connected");
    
    SystemEvent event(SystemEvent::Type::NETWORK_CONNECTED, "Network connected");
    if (wifiManager) {
        event.data["ipAddress"] = wifiManager->getIPAddress();
    }
    publishEvent(event);
    
    // Try to connect OCPP client
    if (ocppClient && !ocppClient->isConnected()) {
        ocppClient->connect();
    }
}

void ChargingStationService::onNetworkDisconnected() {
    Serial.println("Network disconnected");
    
    SystemEvent event(SystemEvent::Type::NETWORK_DISCONNECTED, "Network disconnected");
    publishEvent(event);
}

void ChargingStationService::onOCPPConnected() {
    Serial.println("OCPP connected");
    
    SystemEvent event(SystemEvent::Type::OCPP_CONNECTED, "OCPP connected");
    publishEvent(event);
    
    // Send status notifications for all connectors
    updateAllConnectorStatuses();
}

void ChargingStationService::onOCPPDisconnected() {
    Serial.println("OCPP disconnected");
    
    SystemEvent event(SystemEvent::Type::OCPP_DISCONNECTED, "OCPP disconnected");
    publishEvent(event);
}

ChargingStationService::Statistics ChargingStationService::getStatistics() const {
    Statistics stats;
    
    if (initialized) {
        auto status = const_cast<ChargingStationService*>(this)->getSystemStatus();
        stats.totalTransactions = status.totalTransactions;
        stats.totalEnergyDelivered = status.totalEnergyDelivered;
        stats.totalUptime = millis();
        
        // Calculate connector usage
        for (const auto& connector : status.connectors) {
            stats.connectorUsage[connector.connectorId] = 0; // Would be tracked over time
        }
    }
    
    return stats;
}

// =============================================================================
// ChargingStationApplication Implementation
// =============================================================================

ChargingStationApplication::ChargingStationApplication(
    Infrastructure::OCPPClient* ocpp,
    Domain::IHardwareController* hw,
    Domain::IWiFiManager* wifi,
    Infrastructure::ICertificateManager* cert,
    std::unique_ptr<UseCaseFactory> factory,
    Domain::IConfigRepository* configRepo,
    Domain::ITransactionRepository* transactionRepo
) : ocppClient(ocpp), hardware(hw), wifiManager(wifi), certManager(cert) {
    
    // Create services
    chargingService = std::make_unique<ChargingStationService>(
        std::move(factory), ocpp, hw, wifi
    );
    
    configService = std::make_unique<ConfigurationService>(
        configRepo, chargingService.get()
    );
    
    transactionService = std::make_unique<TransactionService>(
        transactionRepo, hw, chargingService.get()
    );
    
    securityService = std::make_unique<SecurityService>(
        configRepo, cert
    );
    
    startTime = std::chrono::steady_clock::now();
}

ChargingStationApplication::~ChargingStationApplication() {
    shutdown();
}

bool ChargingStationApplication::initialize() {
    if (initialized) {
        return true;
    }
    
    Serial.println("Initializing Charging Station Application...");
    
    // Initialize all services
    if (!chargingService->initialize()) {
        Serial.println("Failed to initialize charging service");
        return false;
    }
    
    initialized = true;
    Serial.println("Charging Station Application initialized successfully");
    
    return true;
}

void ChargingStationApplication::shutdown() {
    if (!initialized) {
        return;
    }
    
    Serial.println("Shutting down Charging Station Application...");
    
    // Shutdown services
    if (chargingService) {
        chargingService->shutdown();
    }
    
    initialized = false;
    Serial.println("Charging Station Application shutdown complete");
}

void ChargingStationApplication::loop() {
    if (!initialized) {
        return;
    }
    
    // Run all service loops
    if (chargingService) {
        chargingService->loop();
    }
    
    if (transactionService) {
        transactionService->collectPeriodicMeterValues();
    }
}

SystemStatus ChargingStationApplication::getOverallStatus() {
    if (!initialized) {
        return SystemStatus();
    }
    
    return chargingService->getSystemStatus();
}

bool ChargingStationApplication::performStartupSelfTest() {
    Serial.println("Performing startup self-test...");
    
    // Test hardware components
    if (hardware) {
        auto status = chargingService->getSystemStatus();
        for (const auto& connector : status.connectors) {
            if (!chargingService->testConnector(connector.connectorId)) {
                Serial.printf("Connector %d self-test failed\n", connector.connectorId);
                return false;
            }
        }
    }
    
    // Test network connectivity
    if (wifiManager && !wifiManager->isConnected()) {
        Serial.println("Network connectivity test failed");
        return false;
    }
    
    // Test OCPP connectivity
    if (ocppClient && !ocppClient->isConnected()) {
        Serial.println("OCPP connectivity test failed");
        // Note: This might be acceptable if network is not available
    }
    
    Serial.println("Startup self-test completed successfully");
    return true;
}

void ChargingStationApplication::handleCriticalError(const std::string& error) {
    Serial.printf("CRITICAL ERROR: %s\n", error.c_str());
    
    // Stop all transactions
    if (chargingService) {
        chargingService->handleEmergencyStop();
    }
    
    // Log error
    SystemEvent event(SystemEvent::Type::ERROR_OCCURRED, "Critical error: " + error);
    if (chargingService) {
        chargingService->publishEvent(event);
    }
    
    // Could implement additional error recovery mechanisms here
}

std::unique_ptr<ChargingStationApplication> ChargingStationApplication::create(
    Infrastructure::OCPPClient* ocpp,
    Domain::IHardwareController* hw,
    Domain::IWiFiManager* wifi,
    Infrastructure::ICertificateManager* cert,
    Domain::IConfigRepository* configRepo,
    Domain::ITransactionRepository* transactionRepo) {
    
    // Create use case factory
    auto factory = std::make_unique<UseCaseFactory>(
        transactionRepo, hw, configRepo, wifi
    );
    
    // Create application
    auto app = std::make_unique<ChargingStationApplication>(
        ocpp, hw, wifi, cert, std::move(factory), configRepo, transactionRepo
    );
    
    return app;
}

// =============================================================================
// Simplified implementations for other services (for completeness)
// =============================================================================

SystemConfiguration ConfigurationService::getCurrentConfiguration() {
    auto domainConfig = configRepo->loadConfiguration();
    
    SystemConfiguration config;
    config.centralSystemUrl = domainConfig.centralSystemUrl;
    config.chargePointId = domainConfig.chargePointId;
    config.heartbeatInterval = domainConfig.heartbeatInterval;
    config.meterValueSampleInterval = domainConfig.meterValueSampleInterval;
    config.numberOfConnectors = domainConfig.numberOfConnectors;
    config.securityProfile = domainConfig.securityProfile;
    config.authorizeRemoteTxRequests = domainConfig.authorizeRemoteTxRequests;
    config.localAuthorizeOffline = domainConfig.localAuthorizeOffline;
    
    return config;
}

CommandResult<bool> ConfigurationService::updateConfiguration(const SystemConfiguration& config) {
    if (!validateConfiguration(config)) {
        return CommandResult<bool>::Failure("Invalid configuration");
    }
    
    // Convert to domain configuration and save
    Domain::Configuration domainConfig;
    domainConfig.centralSystemUrl = config.centralSystemUrl;
    domainConfig.chargePointId = config.chargePointId;
    domainConfig.heartbeatInterval = config.heartbeatInterval;
    domainConfig.meterValueSampleInterval = config.meterValueSampleInterval;
    domainConfig.numberOfConnectors = config.numberOfConnectors;
    domainConfig.securityProfile = config.securityProfile;
    domainConfig.authorizeRemoteTxRequests = config.authorizeRemoteTxRequests;
    domainConfig.localAuthorizeOffline = config.localAuthorizeOffline;
    
    if (!configRepo->saveConfiguration(domainConfig)) {
        return CommandResult<bool>::Failure("Failed to save configuration");
    }
    
    return CommandResult<bool>::Success(true);
}

bool ConfigurationService::validateConfiguration(const SystemConfiguration& config) {
    if (config.chargePointId.empty()) {
        return false;
    }
    
    if (config.centralSystemUrl.empty()) {
        return false;
    }
    
    if (config.heartbeatInterval <= 0 || config.heartbeatInterval > 86400) {
        return false;
    }
    
    if (config.numberOfConnectors <= 0 || config.numberOfConnectors > 16) {
        return false;
    }
    
    return true;
}

std::vector<Domain::Transaction> TransactionService::getActiveTransactions() {
    return transactionRepo->getActiveTransactions();
}

void TransactionService::collectPeriodicMeterValues() {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastCollection = std::chrono::duration_cast<std::chrono::seconds>(
        now - lastMeterValueCollection
    );
    
    // Collect meter values every 60 seconds (configurable)
    if (timeSinceLastCollection.count() >= 60) {
        auto activeTransactions = getActiveTransactions();
        
        for (const auto& transaction : activeTransactions) {
            collectMeterValues(transaction.transactionId);
        }
        
        lastMeterValueCollection = now;
    }
}

void TransactionService::collectMeterValues(int transactionId) {
    // This would collect and send meter values for the transaction
    Serial.printf("Collecting meter values for transaction %d\n", transactionId);
    
    // Implementation would involve:
    // 1. Get transaction details
    // 2. Collect current meter readings from hardware
    // 3. Send meter values via OCPP
}

CommandResult<AuthorizationResult> SecurityService::authorizeIdTag(const std::string& idTag) {
    // Check local cache first
    auto cacheIt = authCache.find(idTag);
    if (cacheIt != authCache.end()) {
        auto& entry = cacheIt->second;
        if (entry.expiry > std::chrono::system_clock::now()) {
            return CommandResult<AuthorizationResult>::Success(
                AuthorizationResult(entry.status, entry.status == "Accepted")
            );
        }
    }
    
    // Check local authorization list
    if (isIdTagInLocalList(idTag)) {
        AuthorizationResult result("Accepted", true);
        updateAuthorizationCache(idTag, result);
        return CommandResult<AuthorizationResult>::Success(result);
    }
    
    // Default to accepted for demonstration
    AuthorizationResult result("Accepted", true);
    updateAuthorizationCache(idTag, result);
    return CommandResult<AuthorizationResult>::Success(result);
}

bool SecurityService::isIdTagInLocalList(const std::string& idTag) {
    // This would check against a local authorization list
    // For now, accept certain patterns
    return idTag.find("TEST") != std::string::npos || 
           idTag.find("ADMIN") != std::string::npos;
}

void SecurityService::updateAuthorizationCache(const std::string& idTag, const AuthorizationResult& result) {
    AuthCacheEntry entry;
    entry.idTag = idTag;
    entry.status = result.status;
    entry.expiry = std::chrono::system_clock::now() + std::chrono::hours(24);
    entry.parentIdTag = result.parentIdTag;
    
    authCache[idTag] = entry;
}