#include "OCPPClient.h"
#include "messages/CoreProfileHandlers.h"
#include <Arduino.h>
#include "../../config.h"

using namespace Infrastructure;

OCPPClient::OCPPClient(
    std::unique_ptr<ISecureWebSocketClient> wsClient,
    Core::Domain::IConfigRepository* configRepo,
    Core::Domain::ITransactionRepository* transactionRepo,
    Core::Domain::IHardwareController* hardware,
    ICertificateManager* certManager,
    Core::Application::UseCaseFactory* useCaseFactory
) : wsClient(std::move(wsClient)),
    configRepo(configRepo),
    transactionRepo(transactionRepo),
    hardware(hardware),
    certManager(certManager),
    useCaseFactory(useCaseFactory) {
    
    // Initialize stats
    stats.totalMessagesSent = 0;
    stats.totalMessagesReceived = 0;
    stats.totalErrors = 0;
    stats.lastConnectionTime = 0;
    
    // Initialize other members
    connected = false;
    registered = false;
    heartbeatInterval = DEFAULT_HEARTBEAT_INTERVAL * 1000;
    lastHeartbeat = 0;
    messageCounter = 0;
    
    setupMessageHandlers();
}

OCPPClient::~OCPPClient() {
    disconnect();
}

bool OCPPClient::connect() {
    if (connected) {
        Serial.println("Already connected to Central System");
        return true;
    }
    
    // Get central system URL from config
    std::string centralSystemUrl = "ws://localhost:9000/ocpp/CP001";
    if (configRepo) {
        auto config = configRepo->loadConfiguration();
        if (!config.centralSystemUrl.empty()) {
            centralSystemUrl = config.centralSystemUrl;
        }
    }
    
    Serial.printf("Connecting to Central System: %s\n", centralSystemUrl.c_str());
    
    // Configure WebSocket client
    wsClient->setMessageCallback([this](const std::string& message) {
        handleIncomingMessage(message);
    });
    
    wsClient->setConnectionCallback([this](bool /*connectedFlag*/) {
        Serial.println("WebSocket connected");
        connected = true;
        registered = false;
        stats.lastConnectionTime = millis();
        
        // Send BootNotification immediately upon connection
        sendBootNotification();
    });
    
    // Remove the OnDisconnectCallback for now - method doesn't exist
    
    // Attempt connection with security config
    SecurityConfig secConfig;
    secConfig.profile = SecurityProfile::Profile1_NoSecurity;
    
    if (wsClient->connect(centralSystemUrl, secConfig)) {
        Serial.println("Connection successful");
        return true;
    }
    
    Serial.println("Failed to connect to Central System");
    return false;
}

void OCPPClient::disconnect() {
    if (!connected) return;
    
    Serial.println("Disconnecting from Central System");
    
    if (wsClient) {
        wsClient->disconnect();
    }
    
    connected = false;
    registered = false;
}

bool OCPPClient::isConnected() const {
    return connected && wsClient && wsClient->isConnected();
}

bool OCPPClient::isRegistered() const {
    return registered;
}

void OCPPClient::setupMessageHandlers() {
    messageHandlers.clear();

    messageHandlers["Authorize"] = std::make_unique<AuthorizeHandler>(configRepo, hardware, useCaseFactory);
    messageHandlers["BootNotification"] = std::make_unique<BootNotificationHandler>(configRepo);
    messageHandlers["StartTransaction"] = std::make_unique<StartTransactionHandler>(useCaseFactory);
    messageHandlers["StopTransaction"] = std::make_unique<StopTransactionHandler>(useCaseFactory);
    messageHandlers["StatusNotification"] = std::make_unique<StatusNotificationHandler>(useCaseFactory);
    messageHandlers["MeterValues"] = std::make_unique<MeterValuesHandler>(useCaseFactory);
    messageHandlers["Heartbeat"] = std::make_unique<HeartbeatHandler>();

    Serial.println("Message handlers setup complete");
}

void OCPPClient::handleIncomingMessage(const std::string& message) {
    stats.totalMessagesReceived++;
    
    Serial.printf("Received message: %s\n", message.c_str());
    
    auto parsedMessage = OCPPMessageParser::parseMessage(message);
    if (!parsedMessage) {
        Serial.println("Failed to parse incoming message");
        stats.totalErrors++;
        return;
    }
    
    // Validate message
    if (!OCPPMessageParser::validateMessage(*parsedMessage)) {
        Serial.println("Message validation failed");
        stats.totalErrors++;
        return;
    }
    
    switch (parsedMessage->messageType) {
        case MessageType::CALL:
            handleIncomingCall(*parsedMessage);
            break;
        case MessageType::CALLRESULT:
            handleIncomingCallResult(*parsedMessage);
            break;
        case MessageType::CALLERROR:
            handleIncomingCallError(*parsedMessage);
            break;
        default:
            Serial.println("Unknown message type");
            stats.totalErrors++;
            break;
    }
}

void OCPPClient::handleIncomingCall(OCPPMessage& message) {
    auto handlerIt = messageHandlers.find(message.action);
    if (handlerIt == messageHandlers.end()) {
        Serial.printf("No handler for action: %s\n", message.action.c_str());
        sendCallErrorMessage(message.messageId, ErrorCode::NOT_IMPLEMENTED,
                             "No handler for action", JsonObject());
        return;
    }

    try {
        JsonDocument response = handlerIt->second->handleCall(message.messageId, message.payload.as<JsonObject>());
        JsonObject resultObj = response.to<JsonObject>();
        sendCallResultMessage(message.messageId, resultObj);
    } catch (const std::exception& e) {
        Serial.printf("Error handling CALL %s: %s\n", message.action.c_str(), e.what());
        sendCallErrorMessage(message.messageId, ErrorCode::INTERNAL_ERROR,
                             "Exception during handler execution", JsonObject());
    }
}

void OCPPClient::handleIncomingCallResult(OCPPMessage& message) {
    Serial.printf("Handling CALLRESULT for message ID: %s\n", message.messageId.c_str());
    
    // Find the corresponding pending call
    auto pendingIt = pendingMessages.find(message.messageId);
    if (pendingIt != pendingMessages.end()) {
        // Get the action that was called
        std::string action = pendingIt->second.action;
        
        // Find handler and call handleCallResult
        auto handlerIt = messageHandlers.find(action);
        if (handlerIt != messageHandlers.end()) {
            try {
                handlerIt->second->handleCallResult(message.messageId, message.result.as<JsonObject>());
            } catch (const std::exception& e) {
                Serial.printf("Error handling CALLRESULT for %s: %s\n", action.c_str(), e.what());
            }
        }
        
        // Remove from pending messages
        pendingMessages.erase(pendingIt);
    } else {
        Serial.printf("Received CALLRESULT for unknown message ID: %s\n", message.messageId.c_str());
    }
    
    // Handle specific responses
    if (pendingIt != pendingMessages.end() && pendingIt->second.action == "BootNotification") {
        JsonObject result = message.result.as<JsonObject>();
        if (result.containsKey("status") && result["status"].as<std::string>() == "Accepted") {
            registered = true;
            Serial.println("Charge point registered with Central System");
            
            // Update heartbeat interval if provided
            if (result.containsKey("interval")) {
                heartbeatInterval = result["interval"].as<int>() * 1000; // Convert to milliseconds
            }
        }
    }
}

void OCPPClient::handleIncomingCallError(OCPPMessage& message) {
    Serial.printf("Handling CALLERROR for message ID: %s - %s: %s (stub)\n", 
                  message.messageId.c_str(), message.errorCode.c_str(), message.errorDescription.c_str());
    
    // Find and remove from pending messages
    auto pendingIt = pendingMessages.find(message.messageId);
    if (pendingIt != pendingMessages.end()) {
        pendingMessages.erase(pendingIt);
        stats.totalErrors++;
    }
}

bool OCPPClient::sendCallMessage(const std::string& action, const JsonObject& payload) {
    if (!connected) {
        Serial.println("Cannot send message - not connected");
        return false;
    }
    
    // Generate unique message ID
    std::string messageId = generateMessageId();
    
    // Serialize message
    std::string message = OCPPMessageParser::serializeCall(messageId, action, payload);
    
    Serial.printf("Sending CALL %s: %s\n", action.c_str(), message.c_str());
    
    // Send message
    bool sent = wsClient->sendMessage(message);
    if (sent) {
        // Add to pending messages for response tracking
        JsonDocument payloadCopy;
        payloadCopy.set(payload);
        
        pendingMessages[messageId] = PendingMessage(messageId, action, payloadCopy);
        stats.totalMessagesSent++;
    } else {
        stats.totalErrors++;
    }
    
    return sent;
}

bool OCPPClient::sendCallResultMessage(const std::string& messageId, const JsonObject& result) {
    if (!connected) {
        return false;
    }
    
    std::string message = OCPPMessageParser::serializeCallResult(messageId, result);
    
    Serial.printf("Sending CALLRESULT: %s\n", message.c_str());
    
    bool sent = wsClient->sendMessage(message);
    if (sent) {
        stats.totalMessagesSent++;
    } else {
        stats.totalErrors++;
    }
    
    return sent;
}

bool OCPPClient::sendCallErrorMessage(const std::string& messageId, const std::string& errorCode,
                                     const std::string& errorDescription, const JsonObject& errorDetails) {
    if (!connected) {
        return false;
    }
    
    std::string message = OCPPMessageParser::serializeCallError(messageId, errorCode, errorDescription, errorDetails);
    
    Serial.printf("Sending CALLERROR: %s\n", message.c_str());
    
    bool sent = wsClient->sendMessage(message);
    if (sent) {
        stats.totalMessagesSent++;
    } else {
        stats.totalErrors++;
    }
    
    return sent;
}

bool OCPPClient::sendBootNotification() {
    JsonDocument payload;
    JsonObject payloadObj = payload.to<JsonObject>();
    
    payloadObj["chargePointVendor"] = CHARGE_POINT_VENDOR;
    payloadObj["chargePointModel"] = CHARGE_POINT_MODEL;
    payloadObj["chargePointSerialNumber"] = CHARGE_POINT_SERIAL;
    payloadObj["firmwareVersion"] = FIRMWARE_VERSION;
    payloadObj["chargeBoxSerialNumber"] = CHARGE_BOX_SERIAL;
    payloadObj["iccid"] = "";
    payloadObj["imsi"] = "";
    payloadObj["meterType"] = "ADC Current Sensor";
    payloadObj["meterSerialNumber"] = "ESP32-001";
    
    return sendCallMessage("BootNotification", payloadObj);
}

bool OCPPClient::sendHeartbeat() {
    JsonDocument payload;
    JsonObject payloadObj = payload.to<JsonObject>();
    // Heartbeat has empty payload
    
    return sendCallMessage("Heartbeat", payloadObj);
}

bool OCPPClient::sendStatusNotification(int connectorId, const std::string& status, const std::string& errorCode) {
    JsonDocument payload;
    JsonObject payloadObj = payload.to<JsonObject>();
    
    payloadObj["connectorId"] = connectorId;
    payloadObj["status"] = status;
    payloadObj["errorCode"] = errorCode;
    payloadObj["timestamp"] = "2024-01-01T12:00:00.000Z"; // Simple timestamp
    payloadObj["info"] = "";
    payloadObj["vendorId"] = "";
    payloadObj["vendorErrorCode"] = "";
    
    return sendCallMessage("StatusNotification", payloadObj);
}

void OCPPClient::loop() {
    if (wsClient) {
        // wsClient->loop(); // Method may not exist on all implementations
    }
    
    if (connected) {
        processHeartbeat();
        retryPendingMessages();
        
        // Check connection timeout
        if (stats.lastConnectionTime > 0) {
            unsigned long now = millis();
            // Add connection monitoring logic here
        }
    }
}

void OCPPClient::processHeartbeat() {
    unsigned long now = millis();
    
    if (registered && (now - lastHeartbeat >= heartbeatInterval)) {
        if (sendHeartbeat()) {
            lastHeartbeat = now;
        }
    }
}

void OCPPClient::retryPendingMessages() {
    unsigned long now = millis();
    
    for (auto it = pendingMessages.begin(); it != pendingMessages.end();) {
        if (now - it->second.timestamp > MESSAGE_TIMEOUT * 1000) {
            Serial.printf("Message timeout for ID: %s\n", it->first.c_str());
            
            // Retry message
            std::string message = OCPPMessageParser::serializeCall(it->second.messageId,
                                                                  it->second.action,
                                                                  it->second.payload.as<JsonObject>());
            if (wsClient->sendMessage(message)) {
                it->second.timestamp = now; // Update timestamp for retry
                ++it;
            } else {
                Serial.printf("Failed to retry message: %s\n", it->first.c_str());
                it = pendingMessages.erase(it);
                stats.totalErrors++;
            }
        } else {
            ++it;
        }
    }
}

void OCPPClient::handleConnectionError() {
    Serial.println("Handling connection error - attempting reconnect");
    
    // Clear pending messages
    pendingMessages.clear();
    
    // Connection error handling logic here
}

void OCPPClient::shutdown() {
    Serial.println("Shutting down OCPP Client");
    messageHandlers.clear();
    pendingMessages.clear();
    disconnect();
}

std::string OCPPClient::getConnectionStatus() {
    if (!connected) return "Disconnected";
    if (!registered) return "Connected";
    return "Registered";
}

SecurityProfile OCPPClient::getActiveSecurityProfile() {
    // Return the currently active security profile
    return SecurityProfile::Profile1_NoSecurity;
}

std::string OCPPClient::generateMessageId() {
    static int messageCounter = 0;
    return "msg_" + std::to_string(++messageCounter);
}
