#include "OCPPUseCases.h"
#include "../../domain/entities/OCPPEntities.h"
#include <Arduino.h>

using namespace Application;
using namespace Domain;

// =============================================================================
// StartTransactionUseCase Implementation
// =============================================================================

CommandResult<TransactionResult> StartTransactionUseCase::execute(const StartTransactionCommand& request) {
    Serial.printf("Starting transaction - Connector: %d, IdTag: %s\n", 
                  request.connectorId, request.idTag.c_str());
    
    // Validate connector
    if (!validateConnector(request.connectorId)) {
        return CommandResult<TransactionResult>::Failure("Invalid connector ID");
    }
    
    // Validate ID tag
    if (!validateIdTag(request.idTag)) {
        return CommandResult<TransactionResult>::Failure("Invalid ID tag format");
    }
    
    // Check connector availability
    if (!checkConnectorAvailability(request.connectorId)) {
        return CommandResult<TransactionResult>::Failure("Connector not available");
    }
    
    // Check if connector is plugged
    if (!hardware->isConnectorPlugged(request.connectorId)) {
        return CommandResult<TransactionResult>::Failure("Connector not plugged");
    }
    
    // Create new transaction
    Transaction transaction(request.connectorId, request.idTag);
    transaction.startMeterValue = request.meterStart;
    transaction.transactionId = random(10000, 99999); // Generate unique ID
    
    // Save transaction to repository
    if (!transactionRepo->saveTransaction(transaction)) {
        return CommandResult<TransactionResult>::Failure("Failed to save transaction");
    }
    
    // Enable charging hardware
    if (!hardware->enableCharging(request.connectorId)) {
        // Rollback transaction
        transactionRepo->removeTransaction(transaction.transactionId);
        return CommandResult<TransactionResult>::Failure("Failed to enable charging hardware");
    }
    
    // Update connector status LED
    hardware->setStatusLED(request.connectorId, "Charging");
    
    // Create successful result
    TransactionResult result;
    result.transactionId = transaction.transactionId;
    result.status = "Accepted";
    result.idTagInfo = "Accepted";
    result.timestamp = request.timestamp;
    
    Serial.printf("Transaction started successfully - ID: %d\n", transaction.transactionId);
    return CommandResult<TransactionResult>::Success(result);
}

bool StartTransactionUseCase::validateConnector(int connectorId) {
    if (connectorId < 1) {
        Serial.println("Connector ID must be >= 1");
        return false;
    }
    
    auto config = configRepo->loadConfiguration();
    if (connectorId > config.numberOfConnectors) {
        Serial.printf("Connector ID %d exceeds configured connectors (%d)\n", 
                     connectorId, config.numberOfConnectors);
        return false;
    }
    
    return true;
}

bool StartTransactionUseCase::validateIdTag(const std::string& idTag) {
    if (idTag.empty()) {
        Serial.println("ID tag cannot be empty");
        return false;
    }
    
    if (idTag.length() < 4 || idTag.length() > 20) {
        Serial.printf("ID tag length invalid: %d (must be 4-20 chars)\n", idTag.length());
        return false;
    }
    
    // Check for valid characters (alphanumeric)
    for (char c : idTag) {
        if (!isalnum(c)) {
            Serial.printf("ID tag contains invalid character: %c\n", c);
            return false;
        }
    }
    
    return true;
}

bool StartTransactionUseCase::checkConnectorAvailability(int connectorId) {
    // Check if there's already an active transaction on this connector
    auto activeTransactions = transactionRepo->getActiveTransactions();
    for (const auto& tx : activeTransactions) {
        if (tx.connectorId == connectorId && tx.isActive) {
            Serial.printf("Connector %d already has active transaction %d\n", 
                         connectorId, tx.transactionId);
            return false;
        }
    }
    
    return true;
}

// =============================================================================
// StopTransactionUseCase Implementation
// =============================================================================

CommandResult<TransactionResult> StopTransactionUseCase::execute(const StopTransactionCommand& request) {
    Serial.printf("Stopping transaction - ID: %d, MeterStop: %d\n", 
                  request.transactionId, request.meterStop);
    
    // Validate transaction exists
    if (!validateTransaction(request.transactionId)) {
        return CommandResult<TransactionResult>::Failure("Transaction not found or already stopped");
    }
    
    // Get transaction from repository
    auto transactionOpt = transactionRepo->getTransaction(request.transactionId);
    if (!transactionOpt.has_value()) {
        return CommandResult<TransactionResult>::Failure("Transaction not found in repository");
    }
    
    Transaction transaction = transactionOpt.value();
    
    // Validate meter value progression
    if (!validateMeterValue(transaction.startMeterValue, request.meterStop)) {
        return CommandResult<TransactionResult>::Failure("Invalid meter stop value");
    }
    
    // Disable charging hardware
    if (!hardware->disableCharging(transaction.connectorId)) {
        Serial.printf("Warning: Failed to disable charging for connector %d\n", transaction.connectorId);
    }
    
    // Update transaction
    transaction.stopMeterValue = request.meterStop;
    transaction.stopReason = request.reason;
    transaction.stopTime = std::chrono::system_clock::now();
    transaction.isActive = false;
    
    // Save updated transaction
    if (!transactionRepo->saveTransaction(transaction)) {
        return CommandResult<TransactionResult>::Failure("Failed to save transaction stop");
    }
    
    // Update connector status LED
    hardware->setStatusLED(transaction.connectorId, "Available");
    
    // Create successful result
    TransactionResult result;
    result.transactionId = transaction.transactionId;
    result.status = "Accepted";
    result.idTagInfo = "Accepted";
    result.timestamp = request.timestamp;
    
    Serial.printf("Transaction stopped successfully - ID: %d\n", transaction.transactionId);
    return CommandResult<TransactionResult>::Success(result);
}

bool StopTransactionUseCase::validateTransaction(int transactionId) {
    auto transactionOpt = transactionRepo->getTransaction(transactionId);
    if (!transactionOpt.has_value()) {
        return false;
    }
    
    // Check if transaction is still active
    if (!transactionOpt.value().isActive) {
        Serial.printf("Transaction %d is already stopped\n", transactionId);
        return false;
    }
    
    return true;
}

bool StopTransactionUseCase::validateMeterValue(int meterStart, int meterStop) {
    if (meterStop < meterStart) {
        Serial.printf("Meter stop (%d) cannot be less than meter start (%d)\n", 
                     meterStop, meterStart);
        return false;
    }
    
    // Check for reasonable energy delivery (not more than 100 kWh per session)
    int energyDelivered = meterStop - meterStart;
    if (energyDelivered > 100000) { // 100 kWh in Wh
        Serial.printf("Energy delivered (%d Wh) seems excessive\n", energyDelivered);
        return false;
    }
    
    return true;
}

// =============================================================================
// AuthorizeUseCase Implementation
// =============================================================================

CommandResult<AuthorizationResult> AuthorizeUseCase::execute(const AuthorizeCommand& request) {
    Serial.printf("Authorizing ID tag: %s\n", request.idTag.c_str());
    
    // Validate ID tag format
    if (!isValidIdTagFormat(request.idTag)) {
        return CommandResult<AuthorizationResult>::Failure("Invalid ID tag format");
    }
    
    // Check local cache first
    AuthorizationResult cacheResult = checkLocalCache(request.idTag);
    if (cacheResult.authorized) {
        Serial.printf("ID tag %s authorized from local cache\n", request.idTag.c_str());
        return CommandResult<AuthorizationResult>::Success(cacheResult);
    }
    
    // For demonstration, we'll accept all properly formatted tags
    // In a real implementation, this would check against a central system or local list
    AuthorizationResult result("Accepted", true);
    result.expiryDate = "2025-12-31T23:59:59Z";
    
    Serial.printf("ID tag %s authorized\n", request.idTag.c_str());
    return CommandResult<AuthorizationResult>::Success(result);
}

AuthorizationResult AuthorizeUseCase::checkLocalCache(const std::string& idTag) {
    // In a real implementation, this would check a local authorization cache
    // For now, we'll implement basic offline authorization
    
    auto config = configRepo->loadConfiguration();
    if (config.localAuthorizeOffline) {
        // Accept known patterns for offline operation
        if (idTag.find("ADMIN") != std::string::npos || 
            idTag.find("TEST") != std::string::npos) {
            return AuthorizationResult("Accepted", true);
        }
    }
    
    return AuthorizationResult("Unknown", false);
}

bool AuthorizeUseCase::isValidIdTagFormat(const std::string& idTag) {
    if (idTag.empty() || idTag.length() > 20) {
        return false;
    }
    
    // Basic validation - alphanumeric characters only
    for (char c : idTag) {
        if (!isalnum(c) && c != '_' && c != '-') {
            return false;
        }
    }
    
    return true;
}

// =============================================================================
// StatusNotificationUseCase Implementation
// =============================================================================

CommandResult<ConnectorStatus> StatusNotificationUseCase::execute(const StatusNotificationCommand& request) {
    Serial.printf("Status notification - Connector: %d, Status: %s\n", 
                  request.connectorId, request.status.c_str());
    
    // Validate connector ID
    auto config = configRepo->loadConfiguration();
    if (request.connectorId < 0 || request.connectorId > config.numberOfConnectors) {
        return CommandResult<ConnectorStatus>::Failure("Invalid connector ID");
    }
    
    // Validate status
    if (!validateConnectorStatus(request.status)) {
        return CommandResult<ConnectorStatus>::Failure("Invalid connector status");
    }
    
    // Validate error code
    if (!validateErrorCode(request.errorCode)) {
        return CommandResult<ConnectorStatus>::Failure("Invalid error code");
    }
    
    // Update hardware LED status
    hardware->setStatusLED(request.connectorId, request.status);
    
    // Create connector status result
    ConnectorStatus status(request.connectorId);
    status.status = request.status;
    status.errorCode = request.errorCode;
    status.info = request.info;
    status.timestamp = request.timestamp;
    status.available = (request.status == "Available");
    status.charging = (request.status == "Charging");
    
    // Check if connector is plugged
    if (request.connectorId > 0) {
        bool plugged = hardware->isConnectorPlugged(request.connectorId);
        status.available = status.available && plugged;
    }
    
    Serial.printf("Connector %d status updated to: %s\n", request.connectorId, request.status.c_str());
    return CommandResult<ConnectorStatus>::Success(status);
}

bool StatusNotificationUseCase::validateConnectorStatus(const std::string& status) {
    std::vector<std::string> validStatuses = {
        "Available", "Preparing", "Charging", "SuspendedEV", 
        "SuspendedEVSE", "Finishing", "Reserved", "Unavailable", "Faulted"
    };
    
    return std::find(validStatuses.begin(), validStatuses.end(), status) != validStatuses.end();
}

bool StatusNotificationUseCase::validateErrorCode(const std::string& errorCode) {
    std::vector<std::string> validErrorCodes = {
        "NoError", "ConnectorLockFailure", "EVCommunicationError", 
        "GroundFailure", "HighTemperature", "InternalError", "LocalListConflict",
        "Other", "OverCurrentFailure", "PowerMeterFailure", "PowerSwitchFailure",
        "ReaderFailure", "ResetFailure", "UnderVoltage", "WeakSignal"
    };
    
    return std::find(validErrorCodes.begin(), validErrorCodes.end(), errorCode) != validErrorCodes.end();
}

// =============================================================================
// MeterValuesUseCase Implementation
// =============================================================================

CommandResult<bool> MeterValuesUseCase::execute(const MeterValuesCommand& request) {
    Serial.printf("Collecting meter values for connector: %d\n", request.connectorId);
    
    // Collect current meter values from hardware
    auto meterValues = collectMeterValues(request.connectorId);
    
    if (meterValues.empty()) {
        return CommandResult<bool>::Failure("No meter values available");
    }
    
    // Validate collected values
    if (!validateMeterValues(meterValues)) {
        return CommandResult<bool>::Failure("Invalid meter values collected");
    }
    
    Serial.printf("Collected %d meter values for connector %d\n", 
                  meterValues.size(), request.connectorId);
    
    return CommandResult<bool>::Success(true);
}

std::vector<MeterValuesCommand::MeterValueData> MeterValuesUseCase::collectMeterValues(int connectorId) {
    std::vector<MeterValuesCommand::MeterValueData> values;
    
    // Collect energy meter value
    float current = hardware->getCurrentMeterValue(connectorId);
    if (current > 0) {
        MeterValuesCommand::MeterValueData energyValue;
        energyValue.value = std::to_string((int)(current * 1000)); // Convert to mA
        energyValue.measurand = "Current.Import";
        energyValue.unit = "A";
        energyValue.location = "Outlet";
        values.push_back(energyValue);
    }
    
    // Add power measurement if available
    // This would be extended based on available hardware measurements
    
    return values;
}

bool MeterValuesUseCase::validateMeterValues(const std::vector<MeterValuesCommand::MeterValueData>& values) {
    for (const auto& value : values) {
        if (value.value.empty()) {
            Serial.println("Empty meter value detected");
            return false;
        }
        
        // Validate numeric values
        try {
            float numValue = std::stof(value.value);
            if (numValue < 0) {
                Serial.printf("Negative meter value: %f\n", numValue);
                return false;
            }
        } catch (const std::exception& e) {
            Serial.printf("Invalid numeric meter value: %s\n", value.value.c_str());
            return false;
        }
    }
    
    return true;
}

// =============================================================================
// RemoteStartTransactionUseCase Implementation
// =============================================================================

CommandResult<TransactionResult> RemoteStartTransactionUseCase::execute(const RemoteStartTransactionCommand& request) {
    Serial.printf("Remote start transaction - Connector: %d, IdTag: %s\n", 
                  request.connectorId, request.idTag.c_str());
    
    // Check if remote start is allowed
    if (!checkRemoteStartPermissions()) {
        return CommandResult<TransactionResult>::Failure("Remote start not authorized");
    }
    
    // Validate request
    if (!validateRemoteStart(request)) {
        return CommandResult<TransactionResult>::Failure("Invalid remote start request");
    }
    
    // Create start transaction command and delegate to StartTransactionUseCase
    StartTransactionCommand startCmd(request.idTag, request.connectorId, 0);
    
    // Use the existing start transaction use case
    StartTransactionUseCase startUseCase(transactionRepo, hardware, configRepo);
    return startUseCase.execute(startCmd);
}

bool RemoteStartTransactionUseCase::validateRemoteStart(const RemoteStartTransactionCommand& request) {
    // Validate connector
    auto config = configRepo->loadConfiguration();
    if (request.connectorId < 1 || request.connectorId > config.numberOfConnectors) {
        Serial.printf("Invalid connector for remote start: %d\n", request.connectorId);
        return false;
    }
    
    // Validate ID tag
    if (request.idTag.empty()) {
        Serial.println("ID tag required for remote start");
        return false;
    }
    
    // Check connector availability
    if (!hardware->isConnectorPlugged(request.connectorId)) {
        Serial.printf("Connector %d not plugged for remote start\n", request.connectorId);
        return false;
    }
    
    return true;
}

bool RemoteStartTransactionUseCase::checkRemoteStartPermissions() {
    auto config = configRepo->loadConfiguration();
    return config.authorizeRemoteTxRequests;
}

// =============================================================================
// RemoteStopTransactionUseCase Implementation  
// =============================================================================

CommandResult<TransactionResult> RemoteStopTransactionUseCase::execute(const RemoteStopTransactionCommand& request) {
    Serial.printf("Remote stop transaction - ID: %d\n", request.transactionId);
    
    // Validate request
    if (!validateRemoteStop(request.transactionId)) {
        return CommandResult<TransactionResult>::Failure("Invalid remote stop request");
    }
    
    // Get transaction to find ID tag
    auto transactionOpt = transactionRepo->getTransaction(request.transactionId);
    if (!transactionOpt.has_value()) {
        return CommandResult<TransactionResult>::Failure("Transaction not found");
    }
    
    Transaction transaction = transactionOpt.value();
    
    // Create stop transaction command and delegate to StopTransactionUseCase
    StopTransactionCommand stopCmd(request.transactionId, transaction.idTag, transaction.stopMeterValue);
    stopCmd.reason = "Remote";
    
    // Use the existing stop transaction use case
    StopTransactionUseCase stopUseCase(transactionRepo, hardware);
    return stopUseCase.execute(stopCmd);
}

bool RemoteStopTransactionUseCase::validateRemoteStop(int transactionId) {
    // Check if transaction exists and is active
    auto transactionOpt = transactionRepo->getTransaction(transactionId);
    if (!transactionOpt.has_value()) {
        Serial.printf("Transaction %d not found for remote stop\n", transactionId);
        return false;
    }
    
    if (!transactionOpt.value().isActive) {
        Serial.printf("Transaction %d is not active for remote stop\n", transactionId);
        return false;
    }
    
    return true;
}

// =============================================================================
// ChangeConfigurationUseCase Implementation
// =============================================================================

CommandResult<bool> ChangeConfigurationUseCase::execute(const ChangeConfigurationCommand& request) {
    Serial.printf("Changing configuration - Key: %s, Value: %s\n", 
                  request.key.c_str(), request.value.c_str());
    
    // Validate configuration key
    if (!validateConfigurationKey(request.key)) {
        return CommandResult<bool>::Failure("Unknown configuration key");
    }
    
    // Check if configuration is read-only
    if (isReadOnlyConfiguration(request.key)) {
        return CommandResult<bool>::Failure("Configuration key is read-only");
    }
    
    // Validate configuration value
    if (!validateConfigurationValue(request.key, request.value)) {
        return CommandResult<bool>::Failure("Invalid configuration value");
    }
    
    // Load current configuration
    auto config = configRepo->loadConfiguration();
    
    // Update specific configuration values
    if (request.key == "HeartbeatInterval") {
        config.heartbeatInterval = std::stoi(request.value);
    } else if (request.key == "MeterValueSampleInterval") {
        config.meterValueSampleInterval = std::stoi(request.value);
    } else if (request.key == "NumberOfConnectors") {
        config.numberOfConnectors = std::stoi(request.value);
    } else if (request.key == "AuthorizeRemoteTxRequests") {
        config.authorizeRemoteTxRequests = (request.value == "true");
    } else if (request.key == "LocalAuthorizeOffline") {
        config.localAuthorizeOffline = (request.value == "true");
    }
    
    // Save updated configuration
    if (!configRepo->saveConfiguration(config)) {
        return CommandResult<bool>::Failure("Failed to save configuration");
    }
    
    Serial.printf("Configuration updated - %s = %s\n", request.key.c_str(), request.value.c_str());
    return CommandResult<bool>::Success(true);
}

bool ChangeConfigurationUseCase::validateConfigurationKey(const std::string& key) {
    std::vector<std::string> validKeys = {
        "HeartbeatInterval", "MeterValueSampleInterval", "NumberOfConnectors",
        "AuthorizeRemoteTxRequests", "LocalAuthorizeOffline", "TransactionMessageAttempts",
        "TransactionMessageRetryInterval", "SupportedFeatureProfiles"
    };
    
    return std::find(validKeys.begin(), validKeys.end(), key) != validKeys.end();
}

bool ChangeConfigurationUseCase::validateConfigurationValue(const std::string& key, const std::string& value) {
    if (key == "HeartbeatInterval" || key == "MeterValueSampleInterval") {
        try {
            int intValue = std::stoi(value);
            return intValue > 0 && intValue <= 86400; // Max 24 hours
        } catch (const std::exception&) {
            return false;
        }
    } else if (key == "NumberOfConnectors") {
        try {
            int intValue = std::stoi(value);
            return intValue > 0 && intValue <= 16; // Reasonable max
        } catch (const std::exception&) {
            return false;
        }
    } else if (key == "AuthorizeRemoteTxRequests" || key == "LocalAuthorizeOffline") {
        return value == "true" || value == "false";
    }
    
    return !value.empty() && value.length() <= 500; // General validation
}

bool ChangeConfigurationUseCase::isReadOnlyConfiguration(const std::string& key) {
    std::vector<std::string> readOnlyKeys = {
        "SupportedFeatureProfiles", // Usually read-only
        "ChargePointVendor",
        "ChargePointModel",
        "FirmwareVersion"
    };
    
    return std::find(readOnlyKeys.begin(), readOnlyKeys.end(), key) != readOnlyKeys.end();
}

// =============================================================================
// GetSystemStatusUseCase Implementation
// =============================================================================

SystemStatus GetSystemStatusUseCase::execute() {
    Serial.println("Collecting system status...");
    
    SystemStatus status;
    
    // Load configuration
    auto config = configRepo->loadConfiguration();
    status.chargePointId = config.chargePointId;
    status.firmwareVersion = "1.0.0"; // Should come from build system
    status.uptime = std::to_string(millis() / 1000) + " seconds";
    
    // Network status
    if (wifiManager) {
        status.network.wifiConnected = wifiManager->isConnected();
        status.network.ipAddress = wifiManager->getIPAddress();
        // Additional network info would be collected here
    }
    
    // Hardware status
    if (hardware) {
        // This would be implemented based on ESP32Hardware interface extensions
        status.hardware.emergencyStop = false; // hardware->isEmergencyStopPressed();
        status.hardware.rfidReaderConnected = true; // Assume connected for now
    }
    
    // Transaction statistics
    auto activeTransactions = transactionRepo->getActiveTransactions();
    status.totalTransactions = activeTransactions.size();
    
    // Connector status
    for (int i = 1; i <= config.numberOfConnectors; i++) {
        ConnectorStatus connectorStatus(i);
        connectorStatus.status = "Available";
        connectorStatus.available = true;
        connectorStatus.charging = false;
        
        if (hardware) {
            connectorStatus.available = hardware->isConnectorPlugged(i);
            // Check if connector has active transaction
            for (const auto& tx : activeTransactions) {
                if (tx.connectorId == i && tx.isActive) {
                    connectorStatus.charging = true;
                    connectorStatus.status = "Charging";
                    connectorStatus.currentTransactionId = tx.transactionId;
                    break;
                }
            }
        }
        
        status.connectors.push_back(connectorStatus);
    }
    
    // Configuration
    status.configuration = SystemConfiguration();
    status.configuration.centralSystemUrl = config.centralSystemUrl;
    status.configuration.chargePointId = config.chargePointId;
    status.configuration.heartbeatInterval = config.heartbeatInterval;
    status.configuration.meterValueSampleInterval = config.meterValueSampleInterval;
    status.configuration.numberOfConnectors = config.numberOfConnectors;
    status.configuration.securityProfile = config.securityProfile;
    status.configuration.authorizeRemoteTxRequests = config.authorizeRemoteTxRequests;
    status.configuration.localAuthorizeOffline = config.localAuthorizeOffline;
    
    Serial.println("System status collected successfully");
    return status;
}