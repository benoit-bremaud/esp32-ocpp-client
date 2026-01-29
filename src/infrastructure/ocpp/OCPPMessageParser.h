#pragma once

#include <string>
#include <memory>
#include <ArduinoJson.h>
#include "IMessageHandler.h" // Contains MessageType and OCPPMessage

namespace Infrastructure {
    
    /**
     * @brief OCPP Message parser for WebSocket communication
     */
    class OCPPMessageParser {
    public:
        static std::unique_ptr<OCPPMessage> parseMessage(const std::string& rawMessage);
        
        static std::string serializeCall(const std::string& messageId, 
                                        const std::string& action, 
                                        const JsonObject& payload);
        
        static std::string serializeCallResult(const std::string& messageId, 
                                             const JsonObject& result);
        
        static std::string serializeCallError(const std::string& messageId,
                                             const std::string& errorCode,
                                             const std::string& errorDescription,
                                             const JsonObject& errorDetails);
        
        static bool validateMessage(const OCPPMessage& message);
        
    private:
        static bool validateMessageFormat(const JsonArray& jsonMessage);
    };
}