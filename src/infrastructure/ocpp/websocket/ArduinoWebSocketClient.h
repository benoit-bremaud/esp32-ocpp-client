#pragma once

#include <WebSocketsClient.h>
#include <WiFiClientSecure.h>
#include <memory>
#include <functional>

#include "../SecurityProfiles.h"

namespace Infrastructure {
    
    /**
     * @brief ESP32-specific WebSocket client with OCPP security profile support
     */
    class ArduinoWebSocketClient : public ISecureWebSocketClient {
    private:
        std::unique_ptr<WebSocketsClient> wsClient;
        std::unique_ptr<WiFiClientSecure> secureClient;
        
        SecurityConfig currentSecurityConfig;
        SecurityProfile activeProfile = SecurityProfile::Profile1_NoSecurity;
        
        std::function<void(const std::string&)> messageCallback;
        std::function<void(bool)> connectionCallback;
        std::function<void(const std::string&)> errorCallback;
        
        bool connected = false;
        bool heartbeatEnabled = false;
        uint32_t heartbeatInterval = 30;
        unsigned long lastHeartbeat = 0;
        
        // Security profile implementations
        bool setupProfile1Connection(const std::string& url);
        bool setupProfile2Connection(const std::string& url, const SecurityConfig& config);
        bool setupProfile3Connection(const std::string& url, const SecurityConfig& config);
        
        // Certificate validation helpers
        bool validateServerCertificate(const std::string& caCert);
        bool loadClientCertificate(const std::string& cert, const std::string& key);
        
        // WebSocket event handlers
        static void webSocketEvent(WStype_t type, uint8_t* payload, size_t length, ArduinoWebSocketClient* client);
        void handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);
        
        // URL parsing
        struct ParsedURL {
            std::string protocol;
            std::string host;
            int port;
            std::string path;
            bool isSecure;
        };
        ParsedURL parseURL(const std::string& url);
        
    public:
        ArduinoWebSocketClient();
        ~ArduinoWebSocketClient() override;
        
        // ISecureWebSocketClient implementation
        bool connect(const std::string& url, const SecurityConfig& securityConfig) override;
        void disconnect() override;
        bool isConnected() override;
        
        bool sendMessage(const std::string& message) override;
        void setMessageCallback(std::function<void(const std::string&)> callback) override;
        void setConnectionCallback(std::function<void(bool)> callback) override;
        void setErrorCallback(std::function<void(const std::string&)> callback) override;
        
        SecurityProfile getActiveSecurityProfile() override;
        std::string getConnectionInfo() override;
        bool isSecureConnection() override;
        
        void enableHeartbeat(uint32_t intervalSeconds) override;
        void disableHeartbeat() override;
        
        // ESP32 specific methods
        void loop(); // Must be called regularly in main loop
        void handleHeartbeat();
    };
}