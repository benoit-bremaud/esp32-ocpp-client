#pragma once

#include "../IMessageHandler.h"
#include "../../../../core/domain/ports/IConfigRepository.h"
#include "../../../../core/domain/ports/IHardwareController.h"
#include "../../../../core/domain/ports/ITransactionRepository.h"

namespace Core::Application {
class UseCaseFactory;
}

namespace Infrastructure {

/**
 * @brief Core Profile - Authorize message handler
 * Validates RFID tags against local cache or requests central system authorization
 */
class AuthorizeHandler : public IMessageHandler {
private:
    Core::Domain::IConfigRepository* configRepo;
    Core::Domain::IHardwareController* hardware;
    Core::Application::UseCaseFactory* useCaseFactory;

public:
    AuthorizeHandler(Core::Domain::IConfigRepository* config,
                     Core::Domain::IHardwareController* hw,
                     Core::Application::UseCaseFactory* factory)
        : configRepo(config), hardware(hw), useCaseFactory(factory) {}

    JsonDocument handleCall(const std::string& messageId, const JsonObject& payload) override;
    void handleCallResult(const std::string& messageId, const JsonObject& payload) override;
    void handleCallError(const std::string& messageId, const std::string& errorCode,
                         const std::string& errorDescription, const JsonObject& errorDetails) override;

    std::string getMessageType() const override { return "Authorize"; }
    std::string getProfile() const override { return "Core"; }
};

/**
 * @brief Core Profile - Boot Notification handler
 * Sends charging station information to central system during startup
 */
class BootNotificationHandler : public IMessageHandler {
private:
    Core::Domain::IConfigRepository* configRepo;

public:
    explicit BootNotificationHandler(Core::Domain::IConfigRepository* config)
        : configRepo(config) {}

    JsonDocument handleCall(const std::string& messageId, const JsonObject& payload) override;
    void handleCallResult(const std::string& messageId, const JsonObject& payload) override;
    void handleCallError(const std::string& messageId, const std::string& errorCode,
                         const std::string& errorDescription, const JsonObject& errorDetails) override;

    std::string getMessageType() const override { return "BootNotification"; }
    std::string getProfile() const override { return "Core"; }
};

/**
 * @brief Core Profile - Start Transaction handler
 * Initiates a new charging transaction
 */
class StartTransactionHandler : public IMessageHandler {
private:
    Core::Application::UseCaseFactory* useCaseFactory;

public:
    explicit StartTransactionHandler(Core::Application::UseCaseFactory* factory)
        : useCaseFactory(factory) {}

    JsonDocument handleCall(const std::string& messageId, const JsonObject& payload) override;
    void handleCallResult(const std::string& messageId, const JsonObject& payload) override;
    void handleCallError(const std::string& messageId, const std::string& errorCode,
                         const std::string& errorDescription, const JsonObject& errorDetails) override;

    std::string getMessageType() const override { return "StartTransaction"; }
    std::string getProfile() const override { return "Core"; }
};

/**
 * @brief Core Profile - Stop Transaction handler
 * Ends a charging transaction and sends final meter values
 */
class StopTransactionHandler : public IMessageHandler {
private:
    Core::Application::UseCaseFactory* useCaseFactory;

public:
    explicit StopTransactionHandler(Core::Application::UseCaseFactory* factory)
        : useCaseFactory(factory) {}

    JsonDocument handleCall(const std::string& messageId, const JsonObject& payload) override;
    void handleCallResult(const std::string& messageId, const JsonObject& payload) override;
    void handleCallError(const std::string& messageId, const std::string& errorCode,
                         const std::string& errorDescription, const JsonObject& errorDetails) override;

    std::string getMessageType() const override { return "StopTransaction"; }
    std::string getProfile() const override { return "Core"; }
};

/**
 * @brief Core Profile - Status Notification handler
 * Sends connector status changes to central system
 */
class StatusNotificationHandler : public IMessageHandler {
private:
    Core::Application::UseCaseFactory* useCaseFactory;

public:
    explicit StatusNotificationHandler(Core::Application::UseCaseFactory* factory)
        : useCaseFactory(factory) {}

    JsonDocument handleCall(const std::string& messageId, const JsonObject& payload) override;
    void handleCallResult(const std::string& messageId, const JsonObject& payload) override;
    void handleCallError(const std::string& messageId, const std::string& errorCode,
                         const std::string& errorDescription, const JsonObject& errorDetails) override;

    std::string getMessageType() const override { return "StatusNotification"; }
    std::string getProfile() const override { return "Core"; }
};

/**
 * @brief Core Profile - Meter Values handler
 * Sends periodic energy meter readings during charging
 */
class MeterValuesHandler : public IMessageHandler {
private:
    Core::Application::UseCaseFactory* useCaseFactory;

public:
    explicit MeterValuesHandler(Core::Application::UseCaseFactory* factory)
        : useCaseFactory(factory) {}

    JsonDocument handleCall(const std::string& messageId, const JsonObject& payload) override;
    void handleCallResult(const std::string& messageId, const JsonObject& payload) override;
    void handleCallError(const std::string& messageId, const std::string& errorCode,
                         const std::string& errorDescription, const JsonObject& errorDetails) override;

    std::string getMessageType() const override { return "MeterValues"; }
    std::string getProfile() const override { return "Core"; }
};

/**
 * @brief Core Profile - Heartbeat handler
 * Sends periodic heartbeat to maintain connection with central system
 */
class HeartbeatHandler : public IMessageHandler {
public:
    JsonDocument handleCall(const std::string& messageId, const JsonObject& payload) override;
    void handleCallResult(const std::string& messageId, const JsonObject& payload) override;
    void handleCallError(const std::string& messageId, const std::string& errorCode,
                         const std::string& errorDescription, const JsonObject& errorDetails) override;

    std::string getMessageType() const override { return "Heartbeat"; }
    std::string getProfile() const override { return "Core"; }
};

} // namespace Infrastructure
