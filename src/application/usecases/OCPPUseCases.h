#pragma once

#include "../../domain/interfaces/IRepositories.h"
#include "../dto/ApplicationDTO.h"
#include <memory>
#include <functional>

namespace Application {
    
    /**
     * @brief Use Cases define business operations in the application layer
     * 
     * Following CLEAN Architecture principles:
     * - Single Responsibility: Each use case handles one business operation
     * - Dependency Inversion: Depends on domain interfaces, not implementations
     * - Open/Closed: Extensible without modification of existing use cases
     */
    
    /**
     * @brief Base interface for all use cases
     */
    template<typename TRequest, typename TResponse>
    class IUseCase {
    public:
        virtual ~IUseCase() = default;
        virtual CommandResult<TResponse> execute(const TRequest& request) = 0;
    };
    
    /**
     * @brief Use case for starting a charging transaction
     */
    class StartTransactionUseCase : public IUseCase<StartTransactionCommand, TransactionResult> {
    private:
        Domain::ITransactionRepository* transactionRepo;
        Domain::IHardwareController* hardware;
        Domain::IConfigRepository* configRepo;
        
        // Validation helpers
        bool validateConnector(int connectorId);
        bool validateIdTag(const std::string& idTag);
        bool checkConnectorAvailability(int connectorId);
        
    public:
        StartTransactionUseCase(
            Domain::ITransactionRepository* transRepo,
            Domain::IHardwareController* hw,
            Domain::IConfigRepository* config
        ) : transactionRepo(transRepo), hardware(hw), configRepo(config) {}
        
        CommandResult<TransactionResult> execute(const StartTransactionCommand& request) override;
    };
    
    /**
     * @brief Use case for stopping a charging transaction
     */
    class StopTransactionUseCase : public IUseCase<StopTransactionCommand, TransactionResult> {
    private:
        Domain::ITransactionRepository* transactionRepo;
        Domain::IHardwareController* hardware;
        
        bool validateTransaction(int transactionId);
        bool validateMeterValue(int meterStart, int meterStop);
        
    public:
        StopTransactionUseCase(
            Domain::ITransactionRepository* transRepo,
            Domain::IHardwareController* hw
        ) : transactionRepo(transRepo), hardware(hw) {}
        
        CommandResult<TransactionResult> execute(const StopTransactionCommand& request) override;
    };
    
    /**
     * @brief Use case for RFID tag authorization
     */
    class AuthorizeUseCase : public IUseCase<AuthorizeCommand, AuthorizationResult> {
    private:
        Domain::IConfigRepository* configRepo;
        
        // Authorization cache and validation
        AuthorizationResult checkLocalCache(const std::string& idTag);
        bool isValidIdTagFormat(const std::string& idTag);
        
    public:
        AuthorizeUseCase(Domain::IConfigRepository* config) : configRepo(config) {}
        
        CommandResult<AuthorizationResult> execute(const AuthorizeCommand& request) override;
    };
    
    /**
     * @brief Use case for connector status management
     */
    class StatusNotificationUseCase : public IUseCase<StatusNotificationCommand, ConnectorStatus> {
    private:
        Domain::IHardwareController* hardware;
        Domain::IConfigRepository* configRepo;
        
        bool validateConnectorStatus(const std::string& status);
        bool validateErrorCode(const std::string& errorCode);
        
    public:
        StatusNotificationUseCase(
            Domain::IHardwareController* hw,
            Domain::IConfigRepository* config
        ) : hardware(hw), configRepo(config) {}
        
        CommandResult<ConnectorStatus> execute(const StatusNotificationCommand& request) override;
    };
    
    /**
     * @brief Use case for meter values collection and reporting
     */
    class MeterValuesUseCase : public IUseCase<MeterValuesCommand, bool> {
    private:
        Domain::IHardwareController* hardware;
        Domain::ITransactionRepository* transactionRepo;
        
        std::vector<MeterValuesCommand::MeterValueData> collectMeterValues(int connectorId);
        bool validateMeterValues(const std::vector<MeterValuesCommand::MeterValueData>& values);
        
    public:
        MeterValuesUseCase(
            Domain::IHardwareController* hw,
            Domain::ITransactionRepository* transRepo
        ) : hardware(hw), transactionRepo(transRepo) {}
        
        CommandResult<bool> execute(const MeterValuesCommand& request) override;
    };
    
    /**
     * @brief Use case for remote transaction start
     */
    class RemoteStartTransactionUseCase : public IUseCase<RemoteStartTransactionCommand, TransactionResult> {
    private:
        Domain::ITransactionRepository* transactionRepo;
        Domain::IHardwareController* hardware;
        Domain::IConfigRepository* configRepo;
        
        bool validateRemoteStart(const RemoteStartTransactionCommand& request);
        bool checkRemoteStartPermissions();
        
    public:
        RemoteStartTransactionUseCase(
            Domain::ITransactionRepository* transRepo,
            Domain::IHardwareController* hw,
            Domain::IConfigRepository* config
        ) : transactionRepo(transRepo), hardware(hw), configRepo(config) {}
        
        CommandResult<TransactionResult> execute(const RemoteStartTransactionCommand& request) override;
    };
    
    /**
     * @brief Use case for remote transaction stop
     */
    class RemoteStopTransactionUseCase : public IUseCase<RemoteStopTransactionCommand, TransactionResult> {
    private:
        Domain::ITransactionRepository* transactionRepo;
        Domain::IHardwareController* hardware;
        
        bool validateRemoteStop(int transactionId);
        
    public:
        RemoteStopTransactionUseCase(
            Domain::ITransactionRepository* transRepo,
            Domain::IHardwareController* hw
        ) : transactionRepo(transRepo), hardware(hw) {}
        
        CommandResult<TransactionResult> execute(const RemoteStopTransactionCommand& request) override;
    };
    
    /**
     * @brief Use case for configuration management
     */
    class ChangeConfigurationUseCase : public IUseCase<ChangeConfigurationCommand, bool> {
    private:
        Domain::IConfigRepository* configRepo;
        
        bool validateConfigurationKey(const std::string& key);
        bool validateConfigurationValue(const std::string& key, const std::string& value);
        bool isReadOnlyConfiguration(const std::string& key);
        
    public:
        ChangeConfigurationUseCase(Domain::IConfigRepository* config) : configRepo(config) {}
        
        CommandResult<bool> execute(const ChangeConfigurationCommand& request) override;
    };
    
    /**
     * @brief Use case for system status retrieval
     */
    class GetSystemStatusUseCase {
    private:
        Domain::IHardwareController* hardware;
        Domain::ITransactionRepository* transactionRepo;
        Domain::IConfigRepository* configRepo;
        Domain::IWiFiManager* wifiManager;
        
    public:
        GetSystemStatusUseCase(
            Domain::IHardwareController* hw,
            Domain::ITransactionRepository* transRepo,
            Domain::IConfigRepository* config,
            Domain::IWiFiManager* wifi
        ) : hardware(hw), transactionRepo(transRepo), configRepo(config), wifiManager(wifi) {}
        
        SystemStatus execute();
    };
    
    /**
     * @brief Factory for creating use case instances with proper dependency injection
     */
    class UseCaseFactory {
    private:
        Domain::ITransactionRepository* transactionRepo;
        Domain::IHardwareController* hardware;
        Domain::IConfigRepository* configRepo;
        Domain::IWiFiManager* wifiManager;
        
    public:
        UseCaseFactory(
            Domain::ITransactionRepository* transRepo,
            Domain::IHardwareController* hw,
            Domain::IConfigRepository* config,
            Domain::IWiFiManager* wifi
        ) : transactionRepo(transRepo), hardware(hw), configRepo(config), wifiManager(wifi) {}
        
        std::unique_ptr<StartTransactionUseCase> createStartTransactionUseCase() {
            return std::make_unique<StartTransactionUseCase>(transactionRepo, hardware, configRepo);
        }
        
        std::unique_ptr<StopTransactionUseCase> createStopTransactionUseCase() {
            return std::make_unique<StopTransactionUseCase>(transactionRepo, hardware);
        }
        
        std::unique_ptr<AuthorizeUseCase> createAuthorizeUseCase() {
            return std::make_unique<AuthorizeUseCase>(configRepo);
        }
        
        std::unique_ptr<StatusNotificationUseCase> createStatusNotificationUseCase() {
            return std::make_unique<StatusNotificationUseCase>(hardware, configRepo);
        }
        
        std::unique_ptr<MeterValuesUseCase> createMeterValuesUseCase() {
            return std::make_unique<MeterValuesUseCase>(hardware, transactionRepo);
        }
        
        std::unique_ptr<RemoteStartTransactionUseCase> createRemoteStartTransactionUseCase() {
            return std::make_unique<RemoteStartTransactionUseCase>(transactionRepo, hardware, configRepo);
        }
        
        std::unique_ptr<RemoteStopTransactionUseCase> createRemoteStopTransactionUseCase() {
            return std::make_unique<RemoteStopTransactionUseCase>(transactionRepo, hardware);
        }
        
        std::unique_ptr<ChangeConfigurationUseCase> createChangeConfigurationUseCase() {
            return std::make_unique<ChangeConfigurationUseCase>(configRepo);
        }
        
        std::unique_ptr<GetSystemStatusUseCase> createGetSystemStatusUseCase() {
            return std::make_unique<GetSystemStatusUseCase>(hardware, transactionRepo, configRepo, wifiManager);
        }
    };
}