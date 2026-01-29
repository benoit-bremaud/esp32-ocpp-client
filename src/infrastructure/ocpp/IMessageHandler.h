#pragma once

#include <string>
#include <memory>
#include <ArduinoJson.h>

// Bring commonly used ArduinoJson types into the global namespace so that
// the rest of the OCPP infrastructure code can use the short type names
// (JsonDocument, JsonObject, JsonArray) without qualification. This keeps
// the existing code compatible with ArduinoJson v7's namespaced types.
using ArduinoJson::JsonDocument;
using ArduinoJson::JsonObject;
using ArduinoJson::JsonArray;

namespace Infrastructure {
    
    /**
     * @brief OCPP message types enumeration
     */
    enum class MessageType {
        CALL = 2,       // Request from CP to CS or CS to CP
        CALLRESULT = 3, // Response to CALL
        CALLERROR = 4   // Error response
    };
    
    /**
     * @brief OCPP error codes as defined in specification
     */
    namespace ErrorCode {
        const std::string NOT_IMPLEMENTED = "NotImplemented";
        const std::string NOT_SUPPORTED = "NotSupported";
        const std::string INTERNAL_ERROR = "InternalError";
        const std::string PROTOCOL_ERROR = "ProtocolError";
        const std::string SECURITY_ERROR = "SecurityError";
        const std::string FORMATION_VIOLATION = "FormationViolation";
        const std::string PROPERTY_CONSTRAINT_VIOLATION = "PropertyConstraintViolation";
        const std::string OCCURENCE_CONSTRAINT_VIOLATION = "OccurenceConstraintViolation";
        const std::string TYPE_CONSTRAINT_VIOLATION = "TypeConstraintViolation";
        const std::string GENERIC_ERROR = "GenericError";
    }
    
    /**
     * @brief OCPP message structure
     */
    struct OCPPMessage {
        MessageType messageType;
        std::string messageId;
        std::string action;         // For CALL messages
        JsonDocument payload;
        
        // For CALLRESULT
        JsonDocument result;
        
        // For CALLERROR
        std::string errorCode;
        std::string errorDescription;
        JsonDocument errorDetails;
        
        OCPPMessage(MessageType type, const std::string& id) 
            : messageType(type), messageId(id) {}
    };
    
    /**
     * @brief Base interface for OCPP message handlers
     */
    class IMessageHandler {
    public:
        virtual ~IMessageHandler() = default;
        
        // Handle incoming message from Central System
        virtual JsonDocument handleCall(const std::string& messageId, const JsonObject& payload) = 0;
        
        // Handle response from Central System
        virtual void handleCallResult(const std::string& messageId, const JsonObject& payload) = 0;
        
        // Handle error from Central System
        virtual void handleCallError(const std::string& messageId, const std::string& errorCode, 
                                   const std::string& errorDescription, const JsonObject& errorDetails) = 0;
        
        virtual std::string getMessageType() const = 0;
        virtual std::string getProfile() const = 0;
    };
    
    // Forward declaration of OCPPMessageParser
    class OCPPMessageParser;
    
} // namespace Infrastructure