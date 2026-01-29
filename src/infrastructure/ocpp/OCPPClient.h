#pragma once

#include "IMessageHandler.h"
#include "OCPPMessageParser.h"
#include "SecurityProfiles.h"
#include "websocket/ArduinoWebSocketClient.h"
#include "../../../core/domain/entities/Configuration.h"
#include "../../../core/domain/ports/IConfigRepository.h"
#include "../../../core/domain/ports/IHardwareController.h"
#include "../../../core/domain/ports/ITransactionRepository.h"
#include "../../../core/application/usecases/OCPPUseCases.h"
#include <map>
#include <memory>
#include <queue>
#include <mutex>

namespace Infrastructure {
    
    /**
     * @brief Pending message for retry logic
     */
    struct PendingMessage {
        std::string messageId;
        std::string action;
        JsonDocument payload;
        unsigned long timestamp;
        int retryCount;
        int maxRetries;
        
        // Default constructor
        PendingMessage() : timestamp(0), retryCount(0), maxRetries(3) {}
        
        PendingMessage(const std::string& id, const std::string& act, const JsonDocument& pay, int maxRet = 3)
            : messageId(id), action(act), timestamp(millis()), retryCount(0), maxRetries(maxRet) {
            payload.set(pay);
        }
    };
    
    /**
     * @brief Main OCPP Client implementing all OCPP 1.6-J functionality
     */
    class OCPPClient {
    private:
        // Core dependencies
        std::unique_ptr<ISecureWebSocketClient> wsClient;
        Core::Domain::IConfigRepository* configRepo;
        Core::Domain::ITransactionRepository* transactionRepo;
        Core::Domain::IHardwareController* hardware;
        ICertificateManager* certManager;
        Core::Application::UseCaseFactory* useCaseFactory;
        
        // Message handling
        std::map<std::string, std::unique_ptr<IMessageHandler>> messageHandlers;
        std::map<std::string, PendingMessage> pendingMessages;
        
        // Connection state
        bool connected = false;
        bool registered = false;
        unsigned long lastHeartbeat = 0;
        unsigned long heartbeatInterval = 300000; // 5 minutes default
        unsigned long lastReconnectAttempt = 0;
        unsigned long reconnectDelayMs = 0;
        unsigned long reconnectDelayInitialMs = 0;
        unsigned long reconnectDelayMaxMs = 0;
        
        // Message ID generation
        int messageCounter = 0;
        std::mutex messageCounterMutex;
        
        // Configuration cache
        Core::Domain::Configuration currentConfig;
        
        // Security configuration
        SecurityConfig securityConfig;
        
        // Internal methods
        void setupMessageHandlers();
        void setupSecurityConfiguration();
        std::string generateMessageId();
        void handleIncomingMessage(const std::string& rawMessage);
        void handleIncomingCall(OCPPMessage& message);
        void handleIncomingCallResult(OCPPMessage& message);
        void handleIncomingCallError(OCPPMessage& message);
        void processHeartbeat();
        void retryPendingMessages();
        void attemptReconnect();
        
        // Connection callbacks
        void onWebSocketConnected(bool connected);
        void onWebSocketMessage(const std::string& message);
        void onWebSocketError(const std::string& error);
        
        // Message sending helpers
        bool sendCallMessage(const std::string& action, const JsonObject& payload);
        bool sendCallResultMessage(const std::string& messageId, const JsonObject& result);
        bool sendCallErrorMessage(const std::string& messageId, const std::string& errorCode, 
                                const std::string& errorDescription, const JsonObject& errorDetails = JsonObject());
        
    public:
        OCPPClient(std::unique_ptr<ISecureWebSocketClient> wsClient,
                   Core::Domain::IConfigRepository* configRepo,
                   Core::Domain::ITransactionRepository* transactionRepo,
                   Core::Domain::IHardwareController* hardware,
                   ICertificateManager* certManager,
                   Core::Application::UseCaseFactory* useCaseFactory);
        
        ~OCPPClient();
        
        void loop(); // Must be called regularly in main loop
        void shutdown();
        
        // Connection management
        bool connect();
        void disconnect();
        bool isConnected() const;
        bool isRegistered() const;
        
        // Core Profile - Charge Point Initiated Messages
        bool sendBootNotification();
        bool sendHeartbeat();
        bool sendStatusNotification(int connectorId, const std::string& status, 
                                  const std::string& errorCode = "NoError");
        
        // Status and diagnostics
        std::string getConnectionStatus();
        SecurityProfile getActiveSecurityProfile();

        // Error handling and recovery
        void handleConnectionError();
        
        // Statistics
        struct Statistics {
            unsigned long totalMessagesSent = 0;
            unsigned long totalMessagesReceived = 0;
            unsigned long totalErrors = 0;
            unsigned long connectionUptime = 0;
            unsigned long lastConnectionTime = 0;
        } stats;
        
        Statistics getStatistics() const { return stats; }
    };
    
} // namespace Infrastructure
