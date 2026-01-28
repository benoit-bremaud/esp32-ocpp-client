#include "OCPPMessageParser.h"
#include <Arduino.h>

namespace Infrastructure {

std::unique_ptr<OCPPMessage> OCPPMessageParser::parseMessage(const std::string& rawMessage) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, rawMessage);
    
    if (error) {
        Serial.printf("JSON parse error: %s\n", error.c_str());
        return nullptr;
    }
    
    if (!doc.is<JsonArray>()) {
        Serial.println("Message is not a JSON array");
        return nullptr;
    }
    
    JsonArray messageArray = doc.as<JsonArray>();
    
    if (!validateMessageFormat(messageArray)) {
        return nullptr;
    }
    
    int messageTypeInt = messageArray[0];
    std::string messageId = messageArray[1].as<std::string>();
    
    auto message = std::make_unique<OCPPMessage>(static_cast<MessageType>(messageTypeInt), messageId);
    
    switch (message->messageType) {
        case MessageType::CALL:
            if (messageArray.size() >= 4) {
                message->action = messageArray[2].as<std::string>();
                message->payload = messageArray[3];
            }
            break;
            
        case MessageType::CALLRESULT:
            if (messageArray.size() >= 3) {
                message->result = messageArray[2];
            }
            break;
            
        case MessageType::CALLERROR:
            if (messageArray.size() >= 5) {
                message->errorCode = messageArray[2].as<std::string>();
                message->errorDescription = messageArray[3].as<std::string>();
                message->errorDetails = messageArray[4];
            }
            break;
    }
    
    return message;
}

std::string OCPPMessageParser::serializeCall(const std::string& messageId, const std::string& action, const JsonObject& payload) {
    JsonDocument doc;
    JsonArray message = doc.to<JsonArray>();
    
    message.add(static_cast<int>(MessageType::CALL));
    message.add(messageId);
    message.add(action);
    message.add(payload);
    
    std::string result;
    serializeJson(doc, result);
    return result;
}

std::string OCPPMessageParser::serializeCallResult(const std::string& messageId, const JsonObject& result) {
    JsonDocument doc;
    JsonArray message = doc.to<JsonArray>();
    
    message.add(static_cast<int>(MessageType::CALLRESULT));
    message.add(messageId);
    message.add(result);
    
    std::string resultStr;
    serializeJson(doc, resultStr);
    return resultStr;
}

std::string OCPPMessageParser::serializeCallError(const std::string& messageId, const std::string& errorCode, 
                                                const std::string& errorDescription, const JsonObject& errorDetails) {
    JsonDocument doc;
    JsonArray message = doc.to<JsonArray>();
    
    message.add(static_cast<int>(MessageType::CALLERROR));
    message.add(messageId);
    message.add(errorCode);
    message.add(errorDescription);
    message.add(errorDetails);
    
    std::string result;
    serializeJson(doc, result);
    return result;
}

bool OCPPMessageParser::validateMessageFormat(const JsonArray& messageArray) {
    if (messageArray.size() < 3) {
        Serial.println("Message array too short");
        return false;
    }
    
    if (!messageArray[0].is<int>()) {
        Serial.println("Message type is not integer");
        return false;
    }
    
    int messageType = messageArray[0];
    if (messageType < 2 || messageType > 4) {
        Serial.printf("Invalid message type: %d\n", messageType);
        return false;
    }
    
    if (!messageArray[1].is<const char*>()) {
        Serial.println("Message ID is not string");
        return false;
    }
    
    // Validate message structure based on type
    switch (messageType) {
        case static_cast<int>(MessageType::CALL):
            if (messageArray.size() != 4) {
                Serial.println("CALL message must have 4 elements");
                return false;
            }
            if (!messageArray[2].is<const char*>()) {
                Serial.println("Action must be string");
                return false;
            }
            break;
            
        case static_cast<int>(MessageType::CALLRESULT):
            if (messageArray.size() != 3) {
                Serial.println("CALLRESULT message must have 3 elements");
                return false;
            }
            break;
            
        case static_cast<int>(MessageType::CALLERROR):
            if (messageArray.size() != 5) {
                Serial.println("CALLERROR message must have 5 elements");
                return false;
            }
            if (!messageArray[2].is<const char*>() || !messageArray[3].is<const char*>()) {
                Serial.println("Error code and description must be strings");
                return false;
            }
            break;
    }
    
    return true;
}

} // namespace Infrastructure