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
std::unique_ptr<MockClock> clockSource;
std::unique_ptr<UseCaseFactory> useCaseFactory;

void setUp() {
    configRepo = std::make_unique<MockConfigRepository>();
    transactionRepo = std::make_unique<MockTransactionRepository>();
    hardware = std::make_unique<MockHardwareController>();
    idGenerator = std::make_unique<MockIdGenerator>(5000);
    clockSource = std::make_unique<MockClock>();
    useCaseFactory = std::make_unique<UseCaseFactory>(
        transactionRepo.get(),
        hardware.get(),
        configRepo.get(),
        nullptr,
        idGenerator.get(),
        clockSource.get());
}

void tearDown() {
    useCaseFactory.reset();
    clockSource.reset();
    idGenerator.reset();
    hardware.reset();
    transactionRepo.reset();
    configRepo.reset();
}

void test_complete_transaction_flow() {
    hardware->setConnectorPlugged(1, true);

    StartTransactionCommand startCmd;
    startCmd.connectorId = 1;
    startCmd.idTag = "INTEGRATIONTAG";
    startCmd.meterStart = 1000;

    auto startUseCase = useCaseFactory->createStartTransactionUseCase();
    auto startResult = startUseCase->execute(startCmd);

    TEST_ASSERT_TRUE(startResult.success);
    TEST_ASSERT_GREATER_THAN(0, startResult.data.transactionId);

    StopTransactionCommand stopCmd;
    stopCmd.transactionId = startResult.data.transactionId;
    stopCmd.meterStop = 1500;
    stopCmd.reason = "Local";

    auto stopUseCase = useCaseFactory->createStopTransactionUseCase();
    auto stopResult = stopUseCase->execute(stopCmd);

    TEST_ASSERT_TRUE(stopResult.success);

    auto active = transactionRepo->getActiveTransactions();
    TEST_ASSERT_EQUAL_INT(0, active.size());
}

void test_status_notification_flow() {
    StatusNotificationCommand cmd;
    cmd.connectorId = 1;
    cmd.status = "Available";

    auto useCase = useCaseFactory->createStatusNotificationUseCase();
    auto result = useCase->execute(cmd);

    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_STRING("Available", result.data.status.c_str());
}

static int run_tests() {
    UNITY_BEGIN();
    RUN_TEST(test_complete_transaction_flow);
    RUN_TEST(test_status_notification_flow);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    run_tests();
}

void loop() {}
#else
int main(int, char**) {
    return run_tests();
}
#endif
