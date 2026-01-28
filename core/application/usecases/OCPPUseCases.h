#pragma once

#include "UseCase.h"
#include "../dto/Commands.h"
#include "../dto/Results.h"
#include "../ports/IClock.h"
#include "../ports/IIdGenerator.h"
#include "../../domain/ports/IConfigRepository.h"
#include "../../domain/ports/ITransactionRepository.h"
#include "../../domain/ports/IHardwareController.h"
#include "../../domain/ports/IWiFiManager.h"
#include <memory>
#include <vector>

namespace Core::Application {

class StartTransactionUseCase : public IUseCase<StartTransactionCommand, TransactionResult> {
private:
    Domain::ITransactionRepository* transactionRepo;
    Domain::IHardwareController* hardware;
    Domain::IConfigRepository* configRepo;
    IIdGenerator* idGenerator;

    bool validateConnector(int connectorId);
    bool validateIdTag(const std::string& idTag);
    bool checkConnectorAvailability(int connectorId);

public:
    StartTransactionUseCase(Domain::ITransactionRepository* transRepo,
                            Domain::IHardwareController* hw,
                            Domain::IConfigRepository* config,
                            IIdGenerator* ids)
        : transactionRepo(transRepo), hardware(hw), configRepo(config), idGenerator(ids) {}

    CommandResult<TransactionResult> execute(const StartTransactionCommand& request) override;
};

class StopTransactionUseCase : public IUseCase<StopTransactionCommand, TransactionResult> {
private:
    Domain::ITransactionRepository* transactionRepo;
    Domain::IHardwareController* hardware;
    IClock* clock;

    bool validateTransaction(int transactionId);
    bool validateMeterValue(int meterStart, int meterStop);

public:
    StopTransactionUseCase(Domain::ITransactionRepository* transRepo,
                           Domain::IHardwareController* hw,
                           IClock* clk)
        : transactionRepo(transRepo), hardware(hw), clock(clk) {}

    CommandResult<TransactionResult> execute(const StopTransactionCommand& request) override;
};

class AuthorizeUseCase : public IUseCase<AuthorizeCommand, AuthorizationResult> {
private:
    Domain::IConfigRepository* configRepo;

    AuthorizationResult checkLocalCache(const std::string& idTag);
    bool isValidIdTagFormat(const std::string& idTag);

public:
    explicit AuthorizeUseCase(Domain::IConfigRepository* config) : configRepo(config) {}

    CommandResult<AuthorizationResult> execute(const AuthorizeCommand& request) override;
};

class StatusNotificationUseCase : public IUseCase<StatusNotificationCommand, ConnectorStatus> {
private:
    Domain::IHardwareController* hardware;
    Domain::IConfigRepository* configRepo;

    bool validateConnectorStatus(const std::string& status);
    bool validateErrorCode(const std::string& errorCode);

public:
    StatusNotificationUseCase(Domain::IHardwareController* hw,
                              Domain::IConfigRepository* config)
        : hardware(hw), configRepo(config) {}

    CommandResult<ConnectorStatus> execute(const StatusNotificationCommand& request) override;
};

class MeterValuesUseCase : public IUseCase<MeterValuesCommand, bool> {
private:
    Domain::IHardwareController* hardware;
    Domain::ITransactionRepository* transactionRepo;

    std::vector<MeterValueData> collectMeterValues(int connectorId);
    bool validateMeterValues(const std::vector<MeterValueData>& values);

public:
    MeterValuesUseCase(Domain::IHardwareController* hw,
                       Domain::ITransactionRepository* transRepo)
        : hardware(hw), transactionRepo(transRepo) {}

    CommandResult<bool> execute(const MeterValuesCommand& request) override;
};

class RemoteStartTransactionUseCase : public IUseCase<RemoteStartTransactionCommand, TransactionResult> {
private:
    Domain::ITransactionRepository* transactionRepo;
    Domain::IHardwareController* hardware;
    Domain::IConfigRepository* configRepo;
    IIdGenerator* idGenerator;

    bool validateRemoteStart(const RemoteStartTransactionCommand& request);
    bool checkRemoteStartPermissions();

public:
    RemoteStartTransactionUseCase(Domain::ITransactionRepository* transRepo,
                                  Domain::IHardwareController* hw,
                                  Domain::IConfigRepository* config,
                                  IIdGenerator* ids)
        : transactionRepo(transRepo), hardware(hw), configRepo(config), idGenerator(ids) {}

    CommandResult<TransactionResult> execute(const RemoteStartTransactionCommand& request) override;
};

class RemoteStopTransactionUseCase : public IUseCase<RemoteStopTransactionCommand, TransactionResult> {
private:
    Domain::ITransactionRepository* transactionRepo;
    Domain::IHardwareController* hardware;
    IClock* clock;

    bool validateRemoteStop(int transactionId);

public:
    RemoteStopTransactionUseCase(Domain::ITransactionRepository* transRepo,
                                 Domain::IHardwareController* hw,
                                 IClock* clk)
        : transactionRepo(transRepo), hardware(hw), clock(clk) {}

    CommandResult<TransactionResult> execute(const RemoteStopTransactionCommand& request) override;
};

class ChangeConfigurationUseCase : public IUseCase<ChangeConfigurationCommand, bool> {
private:
    Domain::IConfigRepository* configRepo;

    bool validateConfigurationKey(const std::string& key);
    bool validateConfigurationValue(const std::string& key, const std::string& value);
    bool isReadOnlyConfiguration(const std::string& key);

public:
    explicit ChangeConfigurationUseCase(Domain::IConfigRepository* config) : configRepo(config) {}

    CommandResult<bool> execute(const ChangeConfigurationCommand& request) override;
};

class GetSystemStatusUseCase {
private:
    Domain::IHardwareController* hardware;
    Domain::ITransactionRepository* transactionRepo;
    Domain::IConfigRepository* configRepo;
    Domain::IWiFiManager* wifiManager;
    IClock* clock;

public:
    GetSystemStatusUseCase(Domain::IHardwareController* hw,
                           Domain::ITransactionRepository* transRepo,
                           Domain::IConfigRepository* config,
                           Domain::IWiFiManager* wifi,
                           IClock* clk)
        : hardware(hw), transactionRepo(transRepo), configRepo(config), wifiManager(wifi), clock(clk) {}

    SystemStatus execute();
};

class UseCaseFactory {
private:
    Domain::ITransactionRepository* transactionRepo;
    Domain::IHardwareController* hardware;
    Domain::IConfigRepository* configRepo;
    Domain::IWiFiManager* wifiManager;
    IIdGenerator* idGenerator;
    IClock* clock;

public:
    UseCaseFactory(Domain::ITransactionRepository* transRepo,
                   Domain::IHardwareController* hw,
                   Domain::IConfigRepository* config,
                   Domain::IWiFiManager* wifi,
                   IIdGenerator* ids,
                   IClock* clk)
        : transactionRepo(transRepo), hardware(hw), configRepo(config), wifiManager(wifi),
          idGenerator(ids), clock(clk) {}

    std::unique_ptr<StartTransactionUseCase> createStartTransactionUseCase() {
        return std::make_unique<StartTransactionUseCase>(transactionRepo, hardware, configRepo, idGenerator);
    }

    std::unique_ptr<StopTransactionUseCase> createStopTransactionUseCase() {
        return std::make_unique<StopTransactionUseCase>(transactionRepo, hardware, clock);
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
        return std::make_unique<RemoteStartTransactionUseCase>(transactionRepo, hardware, configRepo, idGenerator);
    }

    std::unique_ptr<RemoteStopTransactionUseCase> createRemoteStopTransactionUseCase() {
        return std::make_unique<RemoteStopTransactionUseCase>(transactionRepo, hardware, clock);
    }

    std::unique_ptr<ChangeConfigurationUseCase> createChangeConfigurationUseCase() {
        return std::make_unique<ChangeConfigurationUseCase>(configRepo);
    }

    std::unique_ptr<GetSystemStatusUseCase> createGetSystemStatusUseCase() {
        return std::make_unique<GetSystemStatusUseCase>(hardware, transactionRepo, configRepo, wifiManager, clock);
    }
};

} // namespace Core::Application
