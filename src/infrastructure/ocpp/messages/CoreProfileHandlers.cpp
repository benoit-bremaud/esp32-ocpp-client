#include "CoreProfileHandlers.h"
#include "../IMessageHandler.h"
#include "../../../../core/application/usecases/OCPPUseCases.h"
#include <Arduino.h>
#include <string>
#include "../../../config.h"

namespace Infrastructure {

using Core::Application::AuthorizeCommand;
using Core::Application::AuthorizeUseCase;
using Core::Application::ChangeConfigurationCommand;
using Core::Application::MeterValuesCommand;
using Core::Application::RemoteStartTransactionCommand;
using Core::Application::RemoteStopTransactionCommand;
using Core::Application::StartTransactionCommand;
using Core::Application::StopTransactionCommand;
using Core::Application::StatusNotificationCommand;
using Core::Application::UseCaseFactory;

JsonDocument AuthorizeHandler::handleCall(const std::string& /*messageId*/, const JsonObject& payload) {
    JsonDocument response;

    if (!payload.containsKey("idTag")) {
        Serial.println("Authorize: missing idTag");
        return response;
    }

    if (!useCaseFactory) {
        Serial.println("Authorize: missing UseCaseFactory");
        return response;
    }

    const std::string idTag = payload["idTag"].as<std::string>();
    auto useCase = useCaseFactory->createAuthorizeUseCase();
    auto result = useCase->execute(AuthorizeCommand(idTag));

    JsonObject responsePayload = response.to<JsonObject>();
    JsonObject idTagInfo = responsePayload["idTagInfo"].to<JsonObject>();

    if (result.success) {
        idTagInfo["status"] = result.data.status;
        if (!result.data.expiryDate.empty()) {
            idTagInfo["expiryDate"] = result.data.expiryDate;
        }
        if (!result.data.parentIdTag.empty()) {
            idTagInfo["parentIdTag"] = result.data.parentIdTag;
        }
    } else {
        idTagInfo["status"] = "Invalid";
    }

    return response;
}

void AuthorizeHandler::handleCallResult(const std::string& messageId, const JsonObject& payload) {
    Serial.printf("Authorize CallResult received for messageId: %s\n", messageId.c_str());

    if (payload.containsKey("idTagInfo")) {
        std::string status = payload["idTagInfo"]["status"].as<std::string>();
        Serial.printf("Authorization status: %s\n", status.c_str());
    }
}

void AuthorizeHandler::handleCallError(const std::string& messageId,
                                       const std::string& errorCode,
                                       const std::string& errorDescription,
                                       const JsonObject& /*errorDetails*/) {
    Serial.printf("Authorize error - ID: %s, Code: %s, Description: %s\n",
                  messageId.c_str(), errorCode.c_str(), errorDescription.c_str());
}

JsonDocument BootNotificationHandler::handleCall(const std::string& /*messageId*/, const JsonObject& /*payload*/) {
    JsonDocument response;
    JsonObject responsePayload = response.to<JsonObject>();

    responsePayload["status"] = "Accepted";
    responsePayload["currentTime"] = "2024-01-01T12:00:00Z";
    responsePayload["interval"] = DEFAULT_HEARTBEAT_INTERVAL;

    return response;
}

void BootNotificationHandler::handleCallResult(const std::string& messageId, const JsonObject& payload) {
    Serial.printf("BootNotification CallResult: %s\n", messageId.c_str());

    if (payload.containsKey("status")) {
        std::string status = payload["status"].as<std::string>();
        Serial.printf("Boot status: %s\n", status.c_str());
    }
}

void BootNotificationHandler::handleCallError(const std::string& messageId,
                                              const std::string& errorCode,
                                              const std::string& errorDescription,
                                              const JsonObject& /*errorDetails*/) {
    Serial.printf("BootNotification error - ID: %s, Code: %s, Description: %s\n",
                  messageId.c_str(), errorCode.c_str(), errorDescription.c_str());
}

JsonDocument StartTransactionHandler::handleCall(const std::string& /*messageId*/, const JsonObject& payload) {
    JsonDocument response;

    if (!payload.containsKey("connectorId") ||
        !payload.containsKey("idTag") ||
        !payload.containsKey("meterStart")) {
        Serial.println("StartTransaction: missing required fields");
        return response;
    }

    if (!useCaseFactory) {
        Serial.println("StartTransaction: missing UseCaseFactory");
        return response;
    }

    StartTransactionCommand cmd;
    cmd.connectorId = payload["connectorId"].as<int>();
    cmd.idTag = payload["idTag"].as<std::string>();
    cmd.meterStart = payload["meterStart"].as<int>();
    if (payload.containsKey("timestamp")) {
        cmd.timestamp = payload["timestamp"].as<std::string>();
    }

    auto useCase = useCaseFactory->createStartTransactionUseCase();
    auto result = useCase->execute(cmd);

    JsonObject responsePayload = response.to<JsonObject>();
    JsonObject idTagInfo = responsePayload["idTagInfo"].to<JsonObject>();

    if (result.success) {
        responsePayload["transactionId"] = result.data.transactionId;
        idTagInfo["status"] = result.data.status.empty() ? "Accepted" : result.data.status;
    } else {
        idTagInfo["status"] = "Rejected";
    }

    return response;
}

void StartTransactionHandler::handleCallResult(const std::string& messageId, const JsonObject& payload) {
    Serial.printf("StartTransaction CallResult: %s\n", messageId.c_str());
    if (payload.containsKey("transactionId")) {
        int transactionId = payload["transactionId"].as<int>();
        Serial.printf("Transaction started with ID: %d\n", transactionId);
    }
}

void StartTransactionHandler::handleCallError(const std::string& messageId,
                                              const std::string& errorCode,
                                              const std::string& errorDescription,
                                              const JsonObject& /*errorDetails*/) {
    Serial.printf("StartTransaction error - ID: %s, Code: %s, Description: %s\n",
                  messageId.c_str(), errorCode.c_str(), errorDescription.c_str());
}

JsonDocument StopTransactionHandler::handleCall(const std::string& /*messageId*/, const JsonObject& payload) {
    JsonDocument response;

    if (!payload.containsKey("transactionId") || !payload.containsKey("meterStop")) {
        Serial.println("StopTransaction: missing required fields");
        return response;
    }

    if (!useCaseFactory) {
        Serial.println("StopTransaction: missing UseCaseFactory");
        return response;
    }

    StopTransactionCommand cmd;
    cmd.transactionId = payload["transactionId"].as<int>();
    cmd.meterStop = payload["meterStop"].as<int>();
    if (payload.containsKey("idTag")) {
        cmd.idTag = payload["idTag"].as<std::string>();
    }
    if (payload.containsKey("reason")) {
        cmd.reason = payload["reason"].as<std::string>();
    }
    if (payload.containsKey("timestamp")) {
        cmd.timestamp = payload["timestamp"].as<std::string>();
    }

    auto useCase = useCaseFactory->createStopTransactionUseCase();
    auto result = useCase->execute(cmd);

    JsonObject responsePayload = response.to<JsonObject>();
    JsonObject idTagInfo = responsePayload["idTagInfo"].to<JsonObject>();

    if (result.success) {
        idTagInfo["status"] = result.data.status.empty() ? "Accepted" : result.data.status;
    } else {
        idTagInfo["status"] = "Rejected";
    }

    return response;
}

void StopTransactionHandler::handleCallResult(const std::string& messageId, const JsonObject& payload) {
    Serial.printf("StopTransaction CallResult: %s\n", messageId.c_str());
    if (payload.containsKey("idTagInfo")) {
        std::string status = payload["idTagInfo"]["status"].as<std::string>();
        Serial.printf("Stop transaction status: %s\n", status.c_str());
    }
}

void StopTransactionHandler::handleCallError(const std::string& messageId,
                                             const std::string& errorCode,
                                             const std::string& errorDescription,
                                             const JsonObject& /*errorDetails*/) {
    Serial.printf("StopTransaction error - ID: %s, Code: %s, Description: %s\n",
                  messageId.c_str(), errorCode.c_str(), errorDescription.c_str());
}

JsonDocument StatusNotificationHandler::handleCall(const std::string& /*messageId*/, const JsonObject& payload) {
    JsonDocument response;

    if (!payload.containsKey("connectorId") || !payload.containsKey("status")) {
        Serial.println("StatusNotification: missing required fields");
        return response;
    }

    if (!useCaseFactory) {
        Serial.println("StatusNotification: missing UseCaseFactory");
        return response;
    }

    StatusNotificationCommand cmd;
    cmd.connectorId = payload["connectorId"].as<int>();
    cmd.status = payload["status"].as<std::string>();
    if (payload.containsKey("errorCode")) {
        cmd.errorCode = payload["errorCode"].as<std::string>();
    }
    if (payload.containsKey("info")) {
        cmd.info = payload["info"].as<std::string>();
    }
    if (payload.containsKey("timestamp")) {
        cmd.timestamp = payload["timestamp"].as<std::string>();
    }
    if (payload.containsKey("vendorId")) {
        cmd.vendorId = payload["vendorId"].as<std::string>();
    }
    if (payload.containsKey("vendorErrorCode")) {
        cmd.vendorErrorCode = payload["vendorErrorCode"].as<std::string>();
    }

    auto useCase = useCaseFactory->createStatusNotificationUseCase();
    auto result = useCase->execute(cmd);

    if (!result.success) {
        Serial.println("StatusNotification use case failed");
    }

    return response; // per OCPP, response is empty object
}

void StatusNotificationHandler::handleCallResult(const std::string& messageId, const JsonObject& payload) {
    Serial.printf("StatusNotification CallResult: %s\n", messageId.c_str());
    (void)payload;
}

void StatusNotificationHandler::handleCallError(const std::string& messageId,
                                                const std::string& errorCode,
                                                const std::string& errorDescription,
                                                const JsonObject& /*errorDetails*/) {
    Serial.printf("StatusNotification error - ID: %s, Code: %s, Description: %s\n",
                  messageId.c_str(), errorCode.c_str(), errorDescription.c_str());
}

JsonDocument MeterValuesHandler::handleCall(const std::string& /*messageId*/, const JsonObject& payload) {
    JsonDocument response;

    if (!payload.containsKey("connectorId")) {
        Serial.println("MeterValues: missing connectorId");
        return response;
    }

    if (!useCaseFactory) {
        Serial.println("MeterValues: missing UseCaseFactory");
        return response;
    }

    MeterValuesCommand cmd(payload["connectorId"].as<int>());
    if (payload.containsKey("transactionId")) {
        cmd.transactionId = payload["transactionId"].as<int>();
    }
    if (payload.containsKey("timestamp")) {
        cmd.timestamp = payload["timestamp"].as<std::string>();
    }

    auto useCase = useCaseFactory->createMeterValuesUseCase();
    auto result = useCase->execute(cmd);

    if (!result.success) {
        Serial.println("MeterValues use case failed");
    }

    return response;
}

void MeterValuesHandler::handleCallResult(const std::string& messageId, const JsonObject& payload) {
    Serial.printf("MeterValues CallResult: %s\n", messageId.c_str());
    (void)payload;
}

void MeterValuesHandler::handleCallError(const std::string& messageId,
                                         const std::string& errorCode,
                                         const std::string& errorDescription,
                                         const JsonObject& /*errorDetails*/) {
    Serial.printf("MeterValues error - ID: %s, Code: %s, Description: %s\n",
                  messageId.c_str(), errorCode.c_str(), errorDescription.c_str());
}

JsonDocument HeartbeatHandler::handleCall(const std::string& /*messageId*/, const JsonObject& /*payload*/) {
    JsonDocument response;
    JsonObject responsePayload = response.to<JsonObject>();
    responsePayload["currentTime"] = "2024-01-01T12:00:00Z";
    return response;
}

void HeartbeatHandler::handleCallResult(const std::string& messageId, const JsonObject& payload) {
    Serial.printf("Heartbeat CallResult: %s\n", messageId.c_str());
    if (payload.containsKey("currentTime")) {
        std::string currentTime = payload["currentTime"].as<std::string>();
        Serial.printf("Heartbeat time: %s\n", currentTime.c_str());
    }
}

void HeartbeatHandler::handleCallError(const std::string& messageId,
                                       const std::string& errorCode,
                                       const std::string& errorDescription,
                                       const JsonObject& /*errorDetails*/) {
    Serial.printf("Heartbeat error - ID: %s, Code: %s, Description: %s\n",
                  messageId.c_str(), errorCode.c_str(), errorDescription.c_str());
}

} // namespace Infrastructure
