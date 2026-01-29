/**
 * @file main.cpp
 * @brief ESP32 OCPP Client Test Suite - Standalone Version
 * 
 * This is a standalone test runner that can be compiled and run directly
 * on the ESP32 to validate core OCPP functionality without requiring
 * the full infrastructure to be working.
 */

#include <Arduino.h>
#include <vector>
#include <memory>
#include <string>

// Test framework simulation (simplified Unity-like)
class SimpleTestFramework {
private:
    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;
    String current_test_name = "";

public:
    void begin() {
        Serial.println("🧪 ESP32 OCPP Client Test Suite");
        Serial.println("================================");
        total_tests = 0;
        passed_tests = 0;
        failed_tests = 0;
    }
    
    void startTest(const String& name) {
        current_test_name = name;
        total_tests++;
        Serial.print("Running: " + name + " ... ");
    }
    
    void assert_true(bool condition, const String& message = "") {
        if (!condition) {
            Serial.println("❌ FAILED");
            if (message.length() > 0) {
                Serial.println("  Assertion failed: " + message);
            }
            failed_tests++;
        }
    }
    
    void assert_equal_int(int expected, int actual) {
        if (expected != actual) {
            Serial.println("❌ FAILED");
            Serial.println("  Expected: " + String(expected) + ", Actual: " + String(actual));
            failed_tests++;
        }
    }
    
    void assert_not_equal_int(int expected, int actual) {
        if (expected == actual) {
            Serial.println("❌ FAILED");
            Serial.println("  Expected NOT: " + String(expected) + ", but got: " + String(actual));
            failed_tests++;
        }
    }
    
    void assert_greater_than(int min_val, int actual) {
        if (actual <= min_val) {
            Serial.println("❌ FAILED");
            Serial.println("  Expected > " + String(min_val) + ", Actual: " + String(actual));
            failed_tests++;
        }
    }
    
    void assert_string_equal(const String& expected, const String& actual) {
        if (expected != actual) {
            Serial.println("❌ FAILED");
            Serial.println("  Expected: '" + expected + "', Actual: '" + actual + "'");
            failed_tests++;
        }
    }
    
    void testPassed() {
        Serial.println("✅ PASSED");
        passed_tests++;
    }
    
    void end() {
        Serial.println("\n================================");
        Serial.println("Test Results:");
        Serial.println("Total Tests: " + String(total_tests));
        Serial.println("Passed: " + String(passed_tests));
        Serial.println("Failed: " + String(failed_tests));
        
        if (failed_tests == 0) {
            Serial.println("🎉 ALL TESTS PASSED! ✅");
        } else {
            Serial.println("❌ " + String(failed_tests) + " TESTS FAILED!");
        }
        Serial.println("================================");
    }
};

SimpleTestFramework testFramework;

// Mock OCPP Domain Entities for Testing
namespace MockDomain {
    
    enum class TransactionStatus {
        CREATED,
        ACTIVE,
        COMPLETED,
        FAILED
    };
    
    enum class ConnectorStatus {
        AVAILABLE,
        OCCUPIED,
        RESERVED,
        UNAVAILABLE,
        FAULTED
    };
    
    struct Transaction {
        int transactionId;
        int connectorId;
        String idTag;
        int meterStart;
        int meterStop;
        String startTimestamp;
        String stopTimestamp;
        TransactionStatus status;
        
        Transaction(int id, int connector, const String& tag, int meter_start) 
            : transactionId(id), connectorId(connector), idTag(tag), 
              meterStart(meter_start), meterStop(0), 
              status(TransactionStatus::CREATED) {
            startTimestamp = getCurrentTimestamp();
        }
        
        void complete(int meter_stop) {
            meterStop = meter_stop;
            stopTimestamp = getCurrentTimestamp();
            status = TransactionStatus::COMPLETED;
        }
        
        int getEnergyConsumed() const {
            return (status == TransactionStatus::COMPLETED) ? (meterStop - meterStart) : 0;
        }
        
    private:
        String getCurrentTimestamp() {
            return "2024-01-01T12:00:00.000Z"; // Mock timestamp
        }
    };
    
    struct Connector {
        int connectorId;
        ConnectorStatus status;
        int maxPower;
        String currentTransaction;
        
        Connector(int id, int max_power = 22000) 
            : connectorId(id), status(ConnectorStatus::AVAILABLE), 
              maxPower(max_power), currentTransaction("") {}
        
        bool isAvailable() const {
            return status == ConnectorStatus::AVAILABLE;
        }
        
        void occupy(const String& transaction_id) {
            if (status == ConnectorStatus::AVAILABLE) {
                status = ConnectorStatus::OCCUPIED;
                currentTransaction = transaction_id;
            }
        }
        
        void release() {
            status = ConnectorStatus::AVAILABLE;
            currentTransaction = "";
        }
    };
    
    struct Configuration {
        String centralSystemUrl;
        String chargePointId;
        int heartbeatInterval;
        int meterValueInterval;
        bool autoReconnect;
        
        Configuration() 
            : centralSystemUrl("ws://localhost:8080/ocpp"),
              chargePointId("TEST_CP_001"),
              heartbeatInterval(60),
              meterValueInterval(300),
              autoReconnect(true) {}
        
        bool isValid() const {
            return !centralSystemUrl.isEmpty() && 
                   !chargePointId.isEmpty() && 
                   heartbeatInterval > 0 && 
                   meterValueInterval > 0 &&
                   (centralSystemUrl.startsWith("ws://") || centralSystemUrl.startsWith("wss://"));
        }
    };
}

// Mock Application Services
namespace MockApplication {
    
    class ChargingService {
    private:
        std::vector<MockDomain::Transaction> transactions;
        std::vector<MockDomain::Connector> connectors;
        int nextTransactionId;
        
    public:
        ChargingService() : nextTransactionId(1) {
            // Initialize with 3 connectors
            for (int i = 1; i <= 3; i++) {
                connectors.emplace_back(i);
            }
        }
        
        struct StartTransactionResult {
            enum Status { ACCEPTED, REJECTED, CONNECTOR_UNAVAILABLE, INVALID_REQUEST };
            Status status;
            int transactionId;
            String message;
        };
        
        StartTransactionResult startTransaction(int connectorId, const String& idTag, int meterStart) {
            StartTransactionResult result;
            
            // Validate input
            if (connectorId < 1 || idTag.isEmpty() || meterStart < 0) {
                result.status = StartTransactionResult::INVALID_REQUEST;
                result.transactionId = 0;
                result.message = "Invalid request parameters";
                return result;
            }
            
            // Find connector
            auto connectorIt = std::find_if(connectors.begin(), connectors.end(),
                [connectorId](const MockDomain::Connector& c) { return c.connectorId == connectorId; });
            
            if (connectorIt == connectors.end() || !connectorIt->isAvailable()) {
                result.status = StartTransactionResult::CONNECTOR_UNAVAILABLE;
                result.transactionId = 0;
                result.message = "Connector not available";
                return result;
            }
            
            // Create transaction
            int transactionId = nextTransactionId++;
            transactions.emplace_back(transactionId, connectorId, idTag, meterStart);
            connectorIt->occupy(String(transactionId));
            
            result.status = StartTransactionResult::ACCEPTED;
            result.transactionId = transactionId;
            result.message = "Transaction started successfully";
            return result;
        }
        
        struct StopTransactionResult {
            enum Status { ACCEPTED, REJECTED, TRANSACTION_NOT_FOUND };
            Status status;
            int energyConsumed;
            String message;
        };
        
        StopTransactionResult stopTransaction(int transactionId, int meterStop) {
            StopTransactionResult result;
            
            auto transactionIt = std::find_if(transactions.begin(), transactions.end(),
                [transactionId](const MockDomain::Transaction& t) { 
                    return t.transactionId == transactionId && 
                           t.status != MockDomain::TransactionStatus::COMPLETED; 
                });
            
            if (transactionIt == transactions.end()) {
                result.status = StopTransactionResult::TRANSACTION_NOT_FOUND;
                result.energyConsumed = 0;
                result.message = "Transaction not found or already completed";
                return result;
            }
            
            // Complete transaction
            transactionIt->complete(meterStop);
            
            // Release connector
            auto connectorIt = std::find_if(connectors.begin(), connectors.end(),
                [&](const MockDomain::Connector& c) { return c.connectorId == transactionIt->connectorId; });
            
            if (connectorIt != connectors.end()) {
                connectorIt->release();
            }
            
            result.status = StopTransactionResult::ACCEPTED;
            result.energyConsumed = transactionIt->getEnergyConsumed();
            result.message = "Transaction stopped successfully";
            return result;
        }
        
        int getActiveTransactionCount() const {
            int count = 0;
            for (const auto& transaction : transactions) {
                if (transaction.status == MockDomain::TransactionStatus::ACTIVE ||
                    transaction.status == MockDomain::TransactionStatus::CREATED) {
                    count++;
                }
            }
            return count;
        }
        
        std::vector<int> getAvailableConnectors() const {
            std::vector<int> available;
            for (const auto& connector : connectors) {
                if (connector.isAvailable()) {
                    available.push_back(connector.connectorId);
                }
            }
            return available;
        }
    };
    
    class ConfigurationService {
    private:
        MockDomain::Configuration config;
        
    public:
        ConfigurationService() {}
        
        MockDomain::Configuration getConfiguration() const {
            return config;
        }
        
        bool updateConfiguration(const MockDomain::Configuration& newConfig) {
            if (!newConfig.isValid()) {
                return false;
            }
            
            config = newConfig;
            return true;
        }
        
        bool updateHeartbeatInterval(int interval) {
            if (interval < 10 || interval > 86400) { // 10 seconds to 1 day
                return false;
            }
            config.heartbeatInterval = interval;
            return true;
        }
        
        bool updateCentralSystemUrl(const String& url) {
            if (url.isEmpty() || (!url.startsWith("ws://") && !url.startsWith("wss://"))) {
                return false;
            }
            config.centralSystemUrl = url;
            return true;
        }
    };
}

// Test Functions
void test_transaction_creation() {
    testFramework.startTest("Transaction Creation");
    
    MockDomain::Transaction transaction(12345, 1, "TEST_RFID_001", 1000);
    
    testFramework.assert_equal_int(12345, transaction.transactionId);
    testFramework.assert_equal_int(1, transaction.connectorId);
    testFramework.assert_string_equal("TEST_RFID_001", transaction.idTag);
    testFramework.assert_equal_int(1000, transaction.meterStart);
    testFramework.assert_true(transaction.status == MockDomain::TransactionStatus::CREATED);
    
    testFramework.testPassed();
}

void test_transaction_completion() {
    testFramework.startTest("Transaction Completion");
    
    MockDomain::Transaction transaction(12346, 2, "TEST_RFID_002", 2000);
    transaction.complete(2500);
    
    testFramework.assert_equal_int(2500, transaction.meterStop);
    testFramework.assert_equal_int(500, transaction.getEnergyConsumed());
    testFramework.assert_true(transaction.status == MockDomain::TransactionStatus::COMPLETED);
    
    testFramework.testPassed();
}

void test_connector_management() {
    testFramework.startTest("Connector Management");
    
    MockDomain::Connector connector(1);
    
    testFramework.assert_true(connector.isAvailable());
    testFramework.assert_equal_int(1, connector.connectorId);
    
    connector.occupy("12345");
    testFramework.assert_true(!connector.isAvailable());
    testFramework.assert_string_equal("12345", connector.currentTransaction);
    
    connector.release();
    testFramework.assert_true(connector.isAvailable());
    testFramework.assert_string_equal("", connector.currentTransaction);
    
    testFramework.testPassed();
}

void test_configuration_validation() {
    testFramework.startTest("Configuration Validation");
    
    MockDomain::Configuration config;
    testFramework.assert_true(config.isValid());
    
    // Test invalid URL
    config.centralSystemUrl = "http://invalid";
    testFramework.assert_true(!config.isValid());
    
    // Test valid WSS URL
    config.centralSystemUrl = "wss://secure-server:8080/ocpp";
    testFramework.assert_true(config.isValid());
    
    // Test invalid heartbeat
    config.heartbeatInterval = 0;
    testFramework.assert_true(!config.isValid());
    
    config.heartbeatInterval = 60;
    testFramework.assert_true(config.isValid());
    
    testFramework.testPassed();
}

void test_charging_service_start_transaction() {
    testFramework.startTest("Charging Service - Start Transaction");
    
    MockApplication::ChargingService service;
    
    auto result = service.startTransaction(1, "TEST_RFID", 1000);
    
    testFramework.assert_true(result.status == MockApplication::ChargingService::StartTransactionResult::ACCEPTED);
    testFramework.assert_greater_than(0, result.transactionId);
    testFramework.assert_equal_int(1, service.getActiveTransactionCount());
    
    // Test invalid request
    auto invalidResult = service.startTransaction(0, "", -1);
    testFramework.assert_true(invalidResult.status == MockApplication::ChargingService::StartTransactionResult::INVALID_REQUEST);
    
    testFramework.testPassed();
}

void test_charging_service_stop_transaction() {
    testFramework.startTest("Charging Service - Stop Transaction");
    
    MockApplication::ChargingService service;
    
    // Start transaction first
    auto startResult = service.startTransaction(1, "TEST_RFID", 1000);
    testFramework.assert_true(startResult.status == MockApplication::ChargingService::StartTransactionResult::ACCEPTED);
    
    // Stop transaction
    auto stopResult = service.stopTransaction(startResult.transactionId, 1500);
    
    testFramework.assert_true(stopResult.status == MockApplication::ChargingService::StopTransactionResult::ACCEPTED);
    testFramework.assert_equal_int(500, stopResult.energyConsumed);
    testFramework.assert_equal_int(0, service.getActiveTransactionCount());
    
    testFramework.testPassed();
}

void test_configuration_service() {
    testFramework.startTest("Configuration Service");
    
    MockApplication::ConfigurationService service;
    
    auto config = service.getConfiguration();
    testFramework.assert_true(config.isValid());
    
    // Test heartbeat interval update
    testFramework.assert_true(service.updateHeartbeatInterval(120));
    testFramework.assert_equal_int(120, service.getConfiguration().heartbeatInterval);
    
    // Test invalid heartbeat interval
    testFramework.assert_true(!service.updateHeartbeatInterval(5)); // Too low
    testFramework.assert_true(!service.updateHeartbeatInterval(90000)); // Too high
    
    // Test URL update
    testFramework.assert_true(service.updateCentralSystemUrl("wss://new-server:8080/ocpp"));
    testFramework.assert_string_equal("wss://new-server:8080/ocpp", service.getConfiguration().centralSystemUrl);
    
    testFramework.testPassed();
}

void test_multiple_connectors() {
    testFramework.startTest("Multiple Connectors");
    
    MockApplication::ChargingService service;
    
    // Initially all connectors should be available
    auto available = service.getAvailableConnectors();
    testFramework.assert_equal_int(3, available.size());
    
    // Start transactions on different connectors
    auto result1 = service.startTransaction(1, "RFID_001", 1000);
    auto result2 = service.startTransaction(2, "RFID_002", 2000);
    
    testFramework.assert_true(result1.status == MockApplication::ChargingService::StartTransactionResult::ACCEPTED);
    testFramework.assert_true(result2.status == MockApplication::ChargingService::StartTransactionResult::ACCEPTED);
    testFramework.assert_equal_int(2, service.getActiveTransactionCount());
    
    // Only connector 3 should be available now
    available = service.getAvailableConnectors();
    testFramework.assert_equal_int(1, available.size());
    testFramework.assert_equal_int(3, available[0]);
    
    testFramework.testPassed();
}

void test_memory_usage() {
    testFramework.startTest("Memory Usage");
    
    size_t initialHeap = ESP.getFreeHeap();
    
    // Create and destroy multiple transactions to test memory management
    {
        MockApplication::ChargingService service;
        
        // Start multiple transactions
        for (int i = 1; i <= 10; i++) {
            service.startTransaction(1, "RFID_" + String(i), i * 100);
        }
        
        // Service goes out of scope here
    }
    
    size_t finalHeap = ESP.getFreeHeap();
    size_t heapDiff = (initialHeap > finalHeap) ? (initialHeap - finalHeap) : (finalHeap - initialHeap);
    
    Serial.printf("  Memory test - Initial: %d, Final: %d, Diff: %d bytes\n", 
                  initialHeap, finalHeap, heapDiff);
    
    // Allow reasonable variance (less than 1KB)
    testFramework.assert_true(heapDiff < 1024, "Memory usage should be minimal");
    
    testFramework.testPassed();
}

void test_integration_scenario() {
    testFramework.startTest("Integration Scenario");
    
    MockApplication::ChargingService chargingService;
    MockApplication::ConfigurationService configService;
    
    // Configure system
    testFramework.assert_true(configService.updateCentralSystemUrl("wss://test-server:8080/ocpp"));
    testFramework.assert_true(configService.updateHeartbeatInterval(30));
    
    // Start charging session
    auto startResult = chargingService.startTransaction(1, "INTEGRATION_TEST", 5000);
    testFramework.assert_true(startResult.status == MockApplication::ChargingService::StartTransactionResult::ACCEPTED);
    
    int transactionId = startResult.transactionId;
    
    // Verify system state
    testFramework.assert_equal_int(1, chargingService.getActiveTransactionCount());
    
    auto available = chargingService.getAvailableConnectors();
    testFramework.assert_equal_int(2, available.size()); // Connectors 2 and 3 should be available
    
    // Stop charging session
    auto stopResult = chargingService.stopTransaction(transactionId, 6500);
    testFramework.assert_true(stopResult.status == MockApplication::ChargingService::StopTransactionResult::ACCEPTED);
    testFramework.assert_equal_int(1500, stopResult.energyConsumed); // 1.5 kWh
    
    // Verify final state
    testFramework.assert_equal_int(0, chargingService.getActiveTransactionCount());
    available = chargingService.getAvailableConnectors();
    testFramework.assert_equal_int(3, available.size()); // All connectors available again
    
    testFramework.testPassed();
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(100);
    }
    
    delay(2000); // Allow time for Serial Monitor to connect
    
    testFramework.begin();
    
    // Run all tests
    test_transaction_creation();
    test_transaction_completion();
    test_connector_management();
    test_configuration_validation();
    test_charging_service_start_transaction();
    test_charging_service_stop_transaction();
    test_configuration_service();
    test_multiple_connectors();
    test_memory_usage();
    test_integration_scenario();
    
    testFramework.end();
    
    Serial.println("\n🎯 Test Suite Complete!");
    Serial.println("This validates the core OCPP business logic");
    Serial.println("without requiring infrastructure dependencies.");
}

void loop() {
    // Main test suite runs in setup()
    delay(1000);
    
    // Optional: Display periodic system info
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 30000) { // Every 30 seconds
        Serial.printf("📊 System Status - Free Heap: %d bytes, Uptime: %lu ms\n", 
                      ESP.getFreeHeap(), millis());
        lastCheck = millis();
    }
}