#include <unity.h>
#include "../../core/application/usecases/OCPPUseCases.h"
#include "../mocks/MockRepositories.h"
#include "../mocks/MockHardwareController.h"
#include "../mocks/MockClockAndId.h"
#include <memory>

using namespace Core::Application;
using namespace MockDomain;
using namespace MockInfrastructure;
using namespace MockApplication;

std::unique_ptr<MockConfigRepository> configRepo;
std::unique_ptr<MockTransactionRepository> transactionRepo;
std::unique_ptr<MockHardwareController> hardware;
std::unique_ptr<MockIdGenerator> idGenerator;
std::unique_ptr<MockClock> clock;
std::unique_ptr<UseCaseFactory> useCaseFactory;

void setUp() {
    configRepo = std::make_unique<MockConfigRepository>();
    transactionRepo = std::make_unique<MockTransactionRepository>();
    hardware = std::make_unique<MockHardwareController>();
    idGenerator = std::make_unique<MockIdGenerator>(1000);
    clock = std::make_unique<MockClock>();
    useCaseFactory = std::make_unique<UseCaseFactory>(
        transactionRepo.get(),
        hardware.get(),
        configRepo.get(),
        nullptr,
        idGenerator.get(),
        clock.get());
}

void tearDown() {
    useCaseFactory.reset();
    clock.reset();
    idGenerator.reset();
    hardware.reset();
    transactionRepo.reset();
    configRepo.reset();
}

void test_start_transaction_success() {
    hardware->setConnectorPlugged(1, true);

    StartTransactionCommand cmd;
    cmd.connectorId = 1;
    cmd.idTag = "RFID1234";
    cmd.meterStart = 100;

    auto useCase = useCaseFactory->createStartTransactionUseCase();
    auto result = useCase->execute(cmd);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_GREATER_THAN(0, result.data.transactionId);

    auto active = transactionRepo->getActiveTransactions();
    TEST_ASSERT_EQUAL_INT(1, active.size());
    TEST_ASSERT_TRUE(hardware->isChargingEnabled(1));
}

void test_start_transaction_invalid_connector() {
    StartTransactionCommand cmd;
    cmd.connectorId = 2;
    cmd.idTag = "RFID1234";
    cmd.meterStart = 100;

    auto useCase = useCaseFactory->createStartTransactionUseCase();
    auto result = useCase->execute(cmd);

    TEST_ASSERT_FALSE(result.success);
}

void test_stop_transaction_success() {
    Core::Domain::Transaction tx(1, "RFID1234");
    tx.transactionId = 2001;
    tx.startMeterValue = 100;
    tx.isActive = true;
    transactionRepo->saveTransaction(tx);

    StopTransactionCommand cmd;
    cmd.transactionId = 2001;
    cmd.meterStop = 150;
    cmd.reason = "Local";

    auto useCase = useCaseFactory->createStopTransactionUseCase();
    auto result = useCase->execute(cmd);

    TEST_ASSERT_TRUE(result.success);

    auto updated = transactionRepo->getTransaction(2001);
    TEST_ASSERT_TRUE(updated.has_value());
    TEST_ASSERT_FALSE(updated->isActive);
    TEST_ASSERT_EQUAL_INT(150, updated->stopMeterValue);
}

void test_authorize_admin_tag() {
    AuthorizeCommand cmd("ADMIN_TAG");
    auto useCase = useCaseFactory->createAuthorizeUseCase();
    auto result = useCase->execute(cmd);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(result.data.authorized);
}

void test_change_configuration() {
    ChangeConfigurationCommand cmd("HeartbeatInterval", "120");
    auto useCase = useCaseFactory->createChangeConfigurationUseCase();
    auto result = useCase->execute(cmd);

    TEST_ASSERT_TRUE(result.success);
    auto cfg = configRepo->loadConfiguration();
    TEST_ASSERT_EQUAL_INT(120, cfg.heartbeatInterval);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_start_transaction_success);
    RUN_TEST(test_start_transaction_invalid_connector);
    RUN_TEST(test_stop_transaction_success);
    RUN_TEST(test_authorize_admin_tag);
    RUN_TEST(test_change_configuration);
    UNITY_END();
}

void loop() {}
