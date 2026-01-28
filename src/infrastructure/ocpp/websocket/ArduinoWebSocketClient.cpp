#include "ArduinoWebSocketClient.h"
#include <Arduino.h>
#include <WiFi.h>

using namespace Infrastructure;

ArduinoWebSocketClient::ArduinoWebSocketClient() {
    wsClient = std::make_unique<WebSocketsClient>();
    secureClient = std::make_unique<WiFiClientSecure>();
}

ArduinoWebSocketClient::~ArduinoWebSocketClient() {
    disconnect();
}

bool ArduinoWebSocketClient::connect(const std::string& url, const SecurityConfig& securityConfig) {
    currentSecurityConfig = securityConfig;
    activeProfile = securityConfig.profile;
    
    Serial.printf("Connecting with Security Profile %d to: %s\n",
                  static_cast<int>(activeProfile), url.c_str());
    
    // Implement connection based on security profile
    switch (activeProfile) {
        case SecurityProfile::Profile1_NoSecurity:
            return setupProfile1Connection(url);
            
        case SecurityProfile::Profile2_TLS:
            return setupProfile2Connection(url, securityConfig);
            
        case SecurityProfile::Profile3_TLS_Client:
            return setupProfile3Connection(url, securityConfig);
            
        default:
            Serial.println("Error: Unknown security profile");
            return false;
    }
}

bool ArduinoWebSocketClient::setupProfile1Connection(const std::string& url) {
    Serial.println("Setting up Profile 1: No Security (Unsecured WebSocket)");
    
    ParsedURL parsedUrl = parseURL(url);
    
    if (parsedUrl.isSecure) {
        Serial.println("Warning: wss:// URL provided but Profile 1 is unsecured");
        return false;
    }
    
    // Setup WebSocket event handler
    wsClient->onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        this->handleWebSocketEvent(type, payload, length);
    });
    
    // Connect to unsecured WebSocket
    wsClient->begin(parsedUrl.host.c_str(), parsedUrl.port, parsedUrl.path.c_str());
    wsClient->setReconnectInterval(5000);
    
    return true;
}

bool ArduinoWebSocketClient::setupProfile2Connection(const std::string& url, const SecurityConfig& config) {
    Serial.println("Setting up Profile 2: TLS with Server Certificate Verification");
    
    ParsedURL parsedUrl = parseURL(url);
    
    if (!parsedUrl.isSecure) {
        Serial.println("Error: Profile 2 requires wss:// URL");
        return false;
    }
    
    // Configure TLS settings
    if (config.verifyServerCertificate && !config.caCertificate.empty()) {
        secureClient->setCACert(config.caCertificate.c_str());
        Serial.println("CA certificate loaded for server verification");
    } else {
        secureClient->setInsecure(); // Skip certificate verification (not recommended)
        Serial.println("Warning: Server certificate verification disabled");
    }
    
    // Optional: Set SNI server name
    if (!config.sniServerName.empty()) {
        // SNI is automatically handled by ESP32's WiFiClientSecure
    }
    
    // Setup WebSocket with secure client
    wsClient->onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        this->handleWebSocketEvent(type, payload, length);
    });
    
    wsClient->beginSSL(parsedUrl.host.c_str(), parsedUrl.port, parsedUrl.path.c_str());
    wsClient->setReconnectInterval(5000);
    
    return true;
}

bool ArduinoWebSocketClient::setupProfile3Connection(const std::string& url, const SecurityConfig& config) {
    Serial.println("Setting up Profile 3: TLS with Client Certificate Authentication (mTLS)");
    
    ParsedURL parsedUrl = parseURL(url);
    
    if (!parsedUrl.isSecure) {
        Serial.println("Error: Profile 3 requires wss:// URL");
        return false;
    }
    
    // Validate required certificates
    if (config.clientCertificate.empty() || config.clientPrivateKey.empty()) {
        Serial.println("Error: Profile 3 requires client certificate and private key");
        return false;
    }
    
    // Load CA certificate for server verification
    if (!config.caCertificate.empty()) {
        secureClient->setCACert(config.caCertificate.c_str());
        Serial.println("CA certificate loaded");
    } else {
        Serial.println("Warning: No CA certificate provided");
        secureClient->setInsecure();
    }
    
    // Load client certificate and private key
    secureClient->setCertificate(config.clientCertificate.c_str());
    secureClient->setPrivateKey(config.clientPrivateKey.c_str());
    Serial.println("Client certificate and private key loaded");
    
    // Setup WebSocket with mutual TLS
    wsClient->onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        this->handleWebSocketEvent(type, payload, length);
    });
    
    wsClient->beginSSL(parsedUrl.host.c_str(), parsedUrl.port, parsedUrl.path.c_str());
    wsClient->setReconnectInterval(5000);
    
    return true;
}

ArduinoWebSocketClient::ParsedURL ArduinoWebSocketClient::parseURL(const std::string& url) {
    ParsedURL result;
    
    // Simple URL parsing for WebSocket URLs
    if (url.find("wss://") == 0) {
        result.protocol = "wss";
        result.isSecure = true;
        std::string remainder = url.substr(6); // Remove "wss://"
        
        size_t pathPos = remainder.find('/');
        if (pathPos != std::string::npos) {
            result.path = remainder.substr(pathPos);
            remainder = remainder.substr(0, pathPos);
        } else {
            result.path = "/";
        }
        
        size_t colonPos = remainder.find(':');
        if (colonPos != std::string::npos) {
            result.host = remainder.substr(0, colonPos);
            result.port = std::stoi(remainder.substr(colonPos + 1));
        } else {
            result.host = remainder;
            result.port = 443; // Default HTTPS port
        }
    } else if (url.find("ws://") == 0) {
        result.protocol = "ws";
        result.isSecure = false;
        std::string remainder = url.substr(5); // Remove "ws://"
        
        size_t pathPos = remainder.find('/');
        if (pathPos != std::string::npos) {
            result.path = remainder.substr(pathPos);
            remainder = remainder.substr(0, pathPos);
        } else {
            result.path = "/";
        }
        
        size_t colonPos = remainder.find(':');
        if (colonPos != std::string::npos) {
            result.host = remainder.substr(0, colonPos);
            result.port = std::stoi(remainder.substr(colonPos + 1));
        } else {
            result.host = remainder;
            result.port = 80; // Default HTTP port
        }
    } else {
        Serial.println("Error: Invalid WebSocket URL format");
    }
    
    return result;
}

void ArduinoWebSocketClient::handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.println("WebSocket Disconnected");
            connected = false;
            if (connectionCallback) {
                connectionCallback(false);
            }
            break;
            
        case WStype_CONNECTED:
            Serial.printf("WebSocket Connected to: %s\
", (char*)payload);
            connected = true;
            if (connectionCallback) {
                connectionCallback(true);
            }
            break;
            
        case WStype_TEXT:
            Serial.printf("Received: %s\
", (char*)payload);
            if (messageCallback) {
                messageCallback(std::string((char*)payload, length));
            }
            break;
            
        case WStype_ERROR:
            Serial.printf("WebSocket Error: %s\
", (char*)payload);
            if (errorCallback) {
                errorCallback(std::string((char*)payload, length));
            }
            break;
            
        case WStype_PING:
            Serial.println("Received ping");
            break;
            
        case WStype_PONG:
            Serial.println("Received pong");
            break;
            
        default:
            break;
    }
}

void ArduinoWebSocketClient::disconnect() {
    if (wsClient) {
        wsClient->disconnect();
    }
    connected = false;
}

bool ArduinoWebSocketClient::isConnected() {
    return connected && wsClient && wsClient->isConnected();
}

bool ArduinoWebSocketClient::sendMessage(const std::string& message) {
    if (!isConnected()) {
        Serial.println("Error: WebSocket not connected");
        return false;
    }
    
    return wsClient->sendTXT(message.c_str());
}

void ArduinoWebSocketClient::setMessageCallback(std::function<void(const std::string&)> callback) {
    messageCallback = callback;
}

void ArduinoWebSocketClient::setConnectionCallback(std::function<void(bool)> callback) {
    connectionCallback = callback;
}

void ArduinoWebSocketClient::setErrorCallback(std::function<void(const std::string&)> callback) {
    errorCallback = callback;
}

SecurityProfile ArduinoWebSocketClient::getActiveSecurityProfile() {
    return activeProfile;
}

std::string ArduinoWebSocketClient::getConnectionInfo() {
    std::string info = "Security Profile: " + std::to_string(static_cast<int>(activeProfile));
    info += ", Connected: " + std::string(connected ? "Yes" : "No");
    info += ", Secure: " + std::string(isSecureConnection() ? "Yes" : "No");
    return info;
}

bool ArduinoWebSocketClient::isSecureConnection() {
    return activeProfile != SecurityProfile::Profile1_NoSecurity;
}

void ArduinoWebSocketClient::enableHeartbeat(uint32_t intervalSeconds) {
    heartbeatEnabled = true;
    heartbeatInterval = intervalSeconds;
    lastHeartbeat = millis();
    Serial.printf("WebSocket heartbeat enabled: %d seconds\
", intervalSeconds);
}

void ArduinoWebSocketClient::disableHeartbeat() {
    heartbeatEnabled = false;
    Serial.println("WebSocket heartbeat disabled");
}

void ArduinoWebSocketClient::loop() {
    if (wsClient) {
        wsClient->loop();
    }
    
    if (heartbeatEnabled) {
        handleHeartbeat();
    }
}

void ArduinoWebSocketClient::handleHeartbeat() {
    unsigned long now = millis();
    if (now - lastHeartbeat >= (heartbeatInterval * 1000)) {
        if (isConnected()) {
            wsClient->sendPing();
            Serial.println("Sent WebSocket ping");
        }
        lastHeartbeat = now;
    }
}
