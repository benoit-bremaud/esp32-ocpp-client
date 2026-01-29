#include "OCPPUseCases.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

using namespace Core::Application;
using namespace Core::Domain;

CommandResult<TransactionResult> StartTransactionUseCase::execute(const StartTransactionCommand& request) {
    if (!validateConnector(request.connectorId)) {
        return CommandResult<TransactionResult>::Failure("Invalid connector ID");
    }
    if (!validateIdTag(request.idTag)) {
        return CommandResult<TransactionResult>::Failure("Invalid ID tag format");
    }
    if (!checkConnectorAvailability(request.connectorId)) {
        return CommandResult<TransactionResult>::Failure("Connector not available");
    }
    if (!hardware->isConnectorPlugged(request.connectorId)) {
        return CommandResult<TransactionResult>::Failure("Connector not plugged");
    }

    Transaction transaction(request.connectorId, request.idTag);
    transaction.startMeterValue = request.meterStart;
    transaction.transactionId = idGenerator ? idGenerator->nextTransactionId() : 0;

    if (transaction.transactionId <= 0) {
        return CommandResult<TransactionResult>::Failure("Failed to generate transaction ID");
    }

    if (!transactionRepo->saveTransaction(transaction)) {
        return CommandResult<TransactionResult>::Failure("Failed to save transaction");
    }

    if (!hardware->enableCharging(request.connectorId)) {
        transactionRepo->removeTransaction(transaction.transactionId);
        return CommandResult<TransactionResult>::Failure("Failed to enable charging hardware");
    }

    hardware->setStatusLED(request.connectorId, "Charging");

    TransactionResult result;
    result.transactionId = transaction.transactionId;
    result.status = "Accepted";
    result.idTagInfo = "Accepted";
    result.timestamp = request.timestamp;

    return CommandResult<TransactionResult>::Success(result);
}

bool StartTransactionUseCase::validateConnector(int connectorId) {
    if (connectorId < 1) {
        return false;
    }

    auto config = configRepo->loadConfiguration();
    if (connectorId > config.numberOfConnectors) {
        return false;
    }

    return true;
}

bool StartTransactionUseCase::validateIdTag(const std::string& idTag) {
    if (idTag.empty()) {
        return false;
    }
    if (idTag.length() < 4 || idTag.length() > 20) {
        return false;
    }

    for (char c : idTag) {
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    return true;
}

bool StartTransactionUseCase::checkConnectorAvailability(int connectorId) {
    auto activeTransactions = transactionRepo->getActiveTransactions();
    for (const auto& tx : activeTransactions) {
        if (tx.connectorId == connectorId && tx.isActive) {
            return false;
        }
    }

    return true;
}

CommandResult<TransactionResult> StopTransactionUseCase::execute(const StopTransactionCommand& request) {
    if (!validateTransaction(request.transactionId)) {
        return CommandResult<TransactionResult>::Failure("Transaction not found or already stopped");
    }

    auto transactionOpt = transactionRepo->getTransaction(request.transactionId);
    if (!transactionOpt.has_value()) {
        return CommandResult<TransactionResult>::Failure("Transaction not found in repository");
    }

    Transaction transaction = transactionOpt.value();

    if (!validateMeterValue(transaction.startMeterValue, request.meterStop)) {
        return CommandResult<TransactionResult>::Failure("Invalid meter stop value");
    }

    if (!hardware->disableCharging(transaction.connectorId)) {
        // Non-fatal: proceed to close transaction
    }

    transaction.stopMeterValue = request.meterStop;
    transaction.stopReason = request.reason;
    transaction.stopTime = clock ? clock->now() : std::chrono::system_clock::now();
    transaction.isActive = false;

    if (!transactionRepo->saveTransaction(transaction)) {
        return CommandResult<TransactionResult>::Failure("Failed to save transaction stop");
    }

    hardware->setStatusLED(transaction.connectorId, "Available");

    TransactionResult result;
    result.transactionId = transaction.transactionId;
    result.status = "Accepted";
    result.idTagInfo = "Accepted";
    result.timestamp = request.timestamp;

    return CommandResult<TransactionResult>::Success(result);
}

bool StopTransactionUseCase::validateTransaction(int transactionId) {
    auto transactionOpt = transactionRepo->getTransaction(transactionId);
    if (!transactionOpt.has_value()) {
        return false;
    }

    if (!transactionOpt.value().isActive) {
        return false;
    }

    return true;
}

bool StopTransactionUseCase::validateMeterValue(int meterStart, int meterStop) {
    if (meterStop < meterStart) {
        return false;
    }

    int energyDelivered = meterStop - meterStart;
    if (energyDelivered > 100000) {
        return false;
    }

    return true;
}

CommandResult<AuthorizationResult> AuthorizeUseCase::execute(const AuthorizeCommand& request) {
    if (!isValidIdTagFormat(request.idTag)) {
        return CommandResult<AuthorizationResult>::Failure("Invalid ID tag format");
    }

    AuthorizationResult cacheResult = checkLocalCache(request.idTag);
    if (cacheResult.authorized) {
        return CommandResult<AuthorizationResult>::Success(cacheResult);
    }

    AuthorizationResult result("Accepted", true);
    result.expiryDate = "2025-12-31T23:59:59Z";

    return CommandResult<AuthorizationResult>::Success(result);
}

AuthorizationResult AuthorizeUseCase::checkLocalCache(const std::string& idTag) {
    auto config = configRepo->loadConfiguration();
    if (config.localAuthorizeOffline) {
        if (idTag.find("ADMIN") != std::string::npos || idTag.find("TEST") != std::string::npos) {
            return AuthorizationResult("Accepted", true);
        }
    }

    return AuthorizationResult("Unknown", false);
}

bool AuthorizeUseCase::isValidIdTagFormat(const std::string& idTag) {
    if (idTag.empty() || idTag.length() > 20) {
        return false;
    }

    for (char c : idTag) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            return false;
        }
    }

    return true;
}

CommandResult<ConnectorStatus> StatusNotificationUseCase::execute(const StatusNotificationCommand& request) {
    auto config = configRepo->loadConfiguration();
    if (request.connectorId < 0 || request.connectorId > config.numberOfConnectors) {
        return CommandResult<ConnectorStatus>::Failure("Invalid connector ID");
    }

    if (!validateConnectorStatus(request.status)) {
        return CommandResult<ConnectorStatus>::Failure("Invalid connector status");
    }

    if (!validateErrorCode(request.errorCode)) {
        return CommandResult<ConnectorStatus>::Failure("Invalid error code");
    }

    hardware->setStatusLED(request.connectorId, request.status);

    ConnectorStatus status(request.connectorId);
    status.status = request.status;
    status.errorCode = request.errorCode;
    status.info = request.info;
    status.timestamp = request.timestamp;
    status.available = (request.status == "Available");
    status.charging = (request.status == "Charging");

    if (request.connectorId > 0) {
        bool plugged = hardware->isConnectorPlugged(request.connectorId);
        status.available = status.available && plugged;
    }

    return CommandResult<ConnectorStatus>::Success(status);
}

bool StatusNotificationUseCase::validateConnectorStatus(const std::string& status) {
    const std::vector<std::string> validStatuses = {
        "Available", "Preparing", "Charging", "SuspendedEV",
        "SuspendedEVSE", "Finishing", "Reserved", "Unavailable", "Faulted"
    };

    return std::find(validStatuses.begin(), validStatuses.end(), status) != validStatuses.end();
}

bool StatusNotificationUseCase::validateErrorCode(const std::string& errorCode) {
    const std::vector<std::string> validErrorCodes = {
        "NoError", "ConnectorLockFailure", "EVCommunicationError",
        "GroundFailure", "HighTemperature", "InternalError", "LocalListConflict",
        "Other", "OverCurrentFailure", "PowerMeterFailure", "PowerSwitchFailure",
        "ReaderFailure", "ResetFailure", "UnderVoltage", "WeakSignal"
    };

    return std::find(validErrorCodes.begin(), validErrorCodes.end(), errorCode) != validErrorCodes.end();
}

CommandResult<bool> MeterValuesUseCase::execute(const MeterValuesCommand& request) {
    auto meterValues = collectMeterValues(request.connectorId);
    if (meterValues.empty()) {
        return CommandResult<bool>::Failure("No meter values available");
    }

    if (!validateMeterValues(meterValues)) {
        return CommandResult<bool>::Failure("Invalid meter values collected");
    }

    return CommandResult<bool>::Success(true);
}

std::vector<MeterValueData> MeterValuesUseCase::collectMeterValues(int connectorId) {
    std::vector<MeterValueData> values;

    float current = hardware->getCurrentMeterValue(connectorId);
    if (current > 0) {
        MeterValueData energyValue;
        energyValue.value = std::to_string(static_cast<int>(current * 1000));
        energyValue.measurand = "Current.Import";
        energyValue.unit = "A";
        energyValue.location = "Outlet";
        values.push_back(energyValue);
    }

    return values;
}

bool MeterValuesUseCase::validateMeterValues(const std::vector<MeterValueData>& values) {
    for (const auto& value : values) {
        if (value.value.empty()) {
            return false;
        }

        try {
            float numValue = std::stof(value.value);
            if (numValue < 0) {
                return false;
            }
        } catch (const std::exception&) {
            return false;
        }
    }

    return true;
}

CommandResult<TransactionResult> RemoteStartTransactionUseCase::execute(const RemoteStartTransactionCommand& request) {
    if (!checkRemoteStartPermissions()) {
        return CommandResult<TransactionResult>::Failure("Remote start not authorized");
    }

    if (!validateRemoteStart(request)) {
        return CommandResult<TransactionResult>::Failure("Invalid remote start request");
    }

    StartTransactionCommand startCmd(request.idTag, request.connectorId, 0);
    StartTransactionUseCase startUseCase(transactionRepo, hardware, configRepo, idGenerator);
    return startUseCase.execute(startCmd);
}

bool RemoteStartTransactionUseCase::validateRemoteStart(const RemoteStartTransactionCommand& request) {
    auto config = configRepo->loadConfiguration();
    if (request.connectorId < 1 || request.connectorId > config.numberOfConnectors) {
        return false;
    }

    if (request.idTag.empty()) {
        return false;
    }

    if (!hardware->isConnectorPlugged(request.connectorId)) {
        return false;
    }

    return true;
}

bool RemoteStartTransactionUseCase::checkRemoteStartPermissions() {
    auto config = configRepo->loadConfiguration();
    return config.authorizeRemoteTxRequests;
}

CommandResult<TransactionResult> RemoteStopTransactionUseCase::execute(const RemoteStopTransactionCommand& request) {
    if (!validateRemoteStop(request.transactionId)) {
        return CommandResult<TransactionResult>::Failure("Invalid remote stop request");
    }

    auto transactionOpt = transactionRepo->getTransaction(request.transactionId);
    if (!transactionOpt.has_value()) {
        return CommandResult<TransactionResult>::Failure("Transaction not found");
    }

    Transaction transaction = transactionOpt.value();

    StopTransactionCommand stopCmd(request.transactionId, transaction.idTag, transaction.stopMeterValue);
    stopCmd.reason = "Remote";

    StopTransactionUseCase stopUseCase(transactionRepo, hardware, clock);
    return stopUseCase.execute(stopCmd);
}

bool RemoteStopTransactionUseCase::validateRemoteStop(int transactionId) {
    auto transactionOpt = transactionRepo->getTransaction(transactionId);
    if (!transactionOpt.has_value()) {
        return false;
    }

    if (!transactionOpt.value().isActive) {
        return false;
    }

    return true;
}

CommandResult<bool> ChangeConfigurationUseCase::execute(const ChangeConfigurationCommand& request) {
    if (!validateConfigurationKey(request.key)) {
        return CommandResult<bool>::Failure("Unknown configuration key");
    }

    if (isReadOnlyConfiguration(request.key)) {
        return CommandResult<bool>::Failure("Configuration key is read-only");
    }

    if (!validateConfigurationValue(request.key, request.value)) {
        return CommandResult<bool>::Failure("Invalid configuration value");
    }

    auto config = configRepo->loadConfiguration();

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

    if (!configRepo->saveConfiguration(config)) {
        return CommandResult<bool>::Failure("Failed to save configuration");
    }

    return CommandResult<bool>::Success(true);
}

bool ChangeConfigurationUseCase::validateConfigurationKey(const std::string& key) {
    const std::vector<std::string> validKeys = {
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
            return intValue > 0 && intValue <= 86400;
        } catch (const std::exception&) {
            return false;
        }
    } else if (key == "NumberOfConnectors") {
        try {
            int intValue = std::stoi(value);
            return intValue > 0 && intValue <= 16;
        } catch (const std::exception&) {
            return false;
        }
    } else if (key == "AuthorizeRemoteTxRequests" || key == "LocalAuthorizeOffline") {
        return value == "true" || value == "false";
    }

    return !value.empty() && value.length() <= 500;
}

bool ChangeConfigurationUseCase::isReadOnlyConfiguration(const std::string& key) {
    const std::vector<std::string> readOnlyKeys = {
        "SupportedFeatureProfiles",
        "ChargePointVendor",
        "ChargePointModel",
        "FirmwareVersion"
    };

    return std::find(readOnlyKeys.begin(), readOnlyKeys.end(), key) != readOnlyKeys.end();
}

SystemStatus GetSystemStatusUseCase::execute() {
    SystemStatus status;

    auto config = configRepo->loadConfiguration();
    status.chargePointId = config.chargePointId;
    status.firmwareVersion = "1.0.0";

    if (clock) {
        auto uptimeSeconds = std::chrono::duration_cast<std::chrono::seconds>(clock->now().time_since_epoch());
        status.uptime = std::to_string(uptimeSeconds.count()) + " seconds";
    }

    if (wifiManager) {
        status.network.wifiConnected = wifiManager->isConnected();
        status.network.ipAddress = wifiManager->getIPAddress();
    }

    if (hardware) {
        status.hardware.emergencyStop = hardware->isEmergencyStopPressed();
    }

    auto activeTransactions = transactionRepo->getActiveTransactions();
    status.totalTransactions = static_cast<int>(activeTransactions.size());

    for (int i = 1; i <= config.numberOfConnectors; i++) {
        ConnectorStatus connectorStatus(i);
        connectorStatus.status = "Available";
        connectorStatus.available = true;
        connectorStatus.charging = false;

        if (hardware) {
            connectorStatus.available = hardware->isConnectorPlugged(i);
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

    status.configuration.centralSystemUrl = config.centralSystemUrl;
    status.configuration.chargePointId = config.chargePointId;
    status.configuration.heartbeatInterval = config.heartbeatInterval;
    status.configuration.meterValueSampleInterval = config.meterValueSampleInterval;
    status.configuration.numberOfConnectors = config.numberOfConnectors;
    status.configuration.securityProfile = config.securityProfile;
    status.configuration.authorizeRemoteTxRequests = config.authorizeRemoteTxRequests;
    status.configuration.localAuthorizeOffline = config.localAuthorizeOffline;

    return status;
}
