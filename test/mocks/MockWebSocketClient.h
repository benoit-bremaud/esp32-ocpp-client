#pragma once

#include "../../src/infrastructure/ocpp/SecurityProfiles.h"
#include <functional>
#include <string>
#include <queue>
#include <map>
#include <vector>

namespace MockInfrastructure {

/**
 * @brief Mock WebSocket Client for Testing OCPP Communication
 * 
 * Simulates WebSocket behavior for testing without actual network connections.
 * Allows controlled testing of message sending, receiving, and connection states.
 */
class MockWebSocketClient : public Infrastructure::ISecureWebSocketClient {
private:
    bool connected = false;
    Infrastructure::SecurityProfile activeProfile = Infrastructure::SecurityProfile::Profile1_NoSecurity;
    Infrastructure::SecurityConfig currentConfig;
    
    // Callback functions
    std::function<void(const std::string&)> messageCallback;
    std::function<void(bool)> connectionCallback;
    std::function<void(const std::string&)> errorCallback;
    
    // Message queues for testing
    std::queue<std::string> incomingMessages;
    std::queue<std::string> outgoingMessages;
    
    // Simulation state
    bool shouldFailConnection = false;
    bool shouldFailSending = false;
    std::string lastConnectedUrl;
    int connectAttempts = 0;
    
public:
    // ISecureWebSocketClient implementation
    bool connect(const std::string& url, const Infrastructure::SecurityConfig& securityConfig) override {
        ++connectAttempts;
        if (shouldFailConnection) {
            return false;
        }
        
        lastConnectedUrl = url;
        currentConfig = securityConfig;
        activeProfile = securityConfig.profile;
        connected = true;
        
        // Simulate async connection callback
        if (connectionCallback) {
            connectionCallback(true);
        }
        
        return true;
    }
    
    void disconnect() override {
        if (connected) {
            connected = false;
            if (connectionCallback) {
                connectionCallback(false);
            }
        }
    }
    
    bool isConnected() const override {
        return connected;
    }
    
    bool sendMessage(const std::string& message) override {
        if (!connected || shouldFailSending) {
            return false;
        }
        
        outgoingMessages.push(message);
        return true;
    }
    
    void setMessageCallback(std::function<void(const std::string&)> callback) override {
        messageCallback = callback;
    }
    
    void setConnectionCallback(std::function<void(bool)> callback) override {
        connectionCallback = callback;
    }
    
    void setErrorCallback(std::function<void(const std::string&)> callback) override {
        errorCallback = callback;
    }
    
    Infrastructure::SecurityProfile getActiveSecurityProfile() const override {
        return activeProfile;
    }
    
    std::string getConnectionInfo() const override {
        return "Mock WebSocket - Connected: " + std::string(connected ? "true" : "false") + 
               ", Profile: " + std::to_string(static_cast<int>(activeProfile));
    }
    
    bool isSecureConnection() const override {
        return activeProfile != Infrastructure::SecurityProfile::Profile1_NoSecurity;
    }
    
    void enableHeartbeat(uint32_t intervalSeconds) override {
        // Mock implementation - just store the interval
        (void)intervalSeconds;
    }
    
    void disableHeartbeat() override {
        // Mock implementation
    }
    
    void loop() override {
        // Process any pending incoming messages
        processIncomingMessages();
    }
    
    // Test helper methods
    void simulateIncomingMessage(const std::string& message) {
        incomingMessages.push(message);
    }
    
    void processIncomingMessages() {
        while (!incomingMessages.empty() && messageCallback) {
            std::string message = incomingMessages.front();
            incomingMessages.pop();
            messageCallback(message);
        }
    }
    
    void simulateConnectionLoss() {
        if (connected) {
            connected = false;
            if (connectionCallback) {
                connectionCallback(false);
            }
        }
    }
    
    void simulateError(const std::string& error) {
        if (errorCallback) {
            errorCallback(error);
        }
    }
    
    void setShouldFailConnection(bool fail) {
        shouldFailConnection = fail;
    }
    
    void setShouldFailSending(bool fail) {
        shouldFailSending = fail;
    }
    
    // Message inspection for testing
    bool hasOutgoingMessage() const {
        return !outgoingMessages.empty();
    }
    
    std::string getNextOutgoingMessage() {
        if (outgoingMessages.empty()) {
            return "";
        }
        std::string message = outgoingMessages.front();
        outgoingMessages.pop();
        return message;
    }
    
    std::vector<std::string> getAllOutgoingMessages() {
        std::vector<std::string> messages;
        while (!outgoingMessages.empty()) {
            messages.push_back(outgoingMessages.front());
            outgoingMessages.pop();
        }
        return messages;
    }
    
    void clearOutgoingMessages() {
        while (!outgoingMessages.empty()) {
            outgoingMessages.pop();
        }
    }
    
    std::string getLastConnectedUrl() const {
        return lastConnectedUrl;
    }

    int getConnectAttempts() const {
        return connectAttempts;
    }
    
    Infrastructure::SecurityConfig getCurrentConfig() const {
        return currentConfig;
    }
    
    void resetMock() {
        connected = false;
        shouldFailConnection = false;
        shouldFailSending = false;
        lastConnectedUrl.clear();
        connectAttempts = 0;
        clearOutgoingMessages();
        
        while (!incomingMessages.empty()) {
            incomingMessages.pop();
        }
    }
};

} // namespace MockInfrastructure
