#include <unity.h>
#include "../../core/domain/entities/Configuration.h"
#include "../../core/domain/entities/Transaction.h"
#include "../../core/domain/entities/Connector.h"
#include "../../core/domain/entities/MeterValue.h"

using namespace Core::Domain;

void setUp() {}
void tearDown() {}

void test_configuration_default_values() {
    Configuration config;
    TEST_ASSERT_GREATER_THAN(0, config.heartbeatInterval);
    TEST_ASSERT_GREATER_THAN(0, config.meterValueSampleInterval);
    TEST_ASSERT_GREATER_THAN(0, config.numberOfConnectors);
}

void test_transaction_entity_creation() {
    Transaction tx(1, "RFID1234");
    tx.transactionId = 42;
    tx.startMeterValue = 100;

    TEST_ASSERT_EQUAL_INT(42, tx.transactionId);
    TEST_ASSERT_EQUAL_INT(1, tx.connectorId);
    TEST_ASSERT_EQUAL_STRING("RFID1234", tx.idTag.c_str());
    TEST_ASSERT_TRUE(tx.isActive);
}

void test_connector_default_status() {
    Connector connector(1);
    TEST_ASSERT_EQUAL_INT(1, connector.connectorId);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Connector::Status::Available),
                          static_cast<int>(connector.status));
}

void test_meter_value_defaults() {
    MeterValue mv;
    SampledValue sample;
    mv.sampledValues.push_back(sample);

    TEST_ASSERT_EQUAL_STRING("Sample.Periodic", mv.sampledValues[0].context.c_str());
    TEST_ASSERT_EQUAL_STRING("Raw", mv.sampledValues[0].format.c_str());
    TEST_ASSERT_EQUAL_STRING("Energy.Active.Import.Register", mv.sampledValues[0].measurand.c_str());
    TEST_ASSERT_EQUAL_STRING("Wh", mv.sampledValues[0].unit.c_str());
}

static int run_tests() {
    UNITY_BEGIN();
    RUN_TEST(test_configuration_default_values);
    RUN_TEST(test_transaction_entity_creation);
    RUN_TEST(test_connector_default_status);
    RUN_TEST(test_meter_value_defaults);
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
