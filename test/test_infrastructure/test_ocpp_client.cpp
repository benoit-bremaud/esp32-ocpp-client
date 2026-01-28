#include <unity.h>
#include "../../src/infrastructure/ocpp/OCPPMessageParser.h"
#include "../mocks/MockWebSocketClient.h"
#include "../mocks/MockFileSystem.h"
#include "../../src/infrastructure/storage/LittleFSConfigRepository.h"
#include "../../src/infrastructure/storage/LittleFSTransactionRepository.h"
#include <ArduinoJson.h>
#include <memory>
#include <chrono>

using namespace Infrastructure;
using namespace MockInfrastructure;
using Infrastructure::LittleFSConfigRepository;
using Infrastructure::LittleFSTransactionRepository;

std::unique_ptr<MockWebSocketClient> mockWebSocket;

void setUp() {
    mockWebSocket = std::make_unique<MockWebSocketClient>();
}

void tearDown() {
    mockWebSocket.reset();
}

void test_parse_call_message() {
    std::string callMessage = R"([2,"12345","BootNotification",{"chargePointModel":"ESP32","chargePointVendor":"Test"}])";

    auto parsedMessage = OCPPMessageParser::parseMessage(callMessage);

    TEST_ASSERT_NOT_NULL(parsedMessage.get());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MessageType::CALL), static_cast<int>(parsedMessage->messageType));
    TEST_ASSERT_EQUAL_STRING("12345", parsedMessage->messageId.c_str());
    TEST_ASSERT_EQUAL_STRING("BootNotification", parsedMessage->action.c_str());

    JsonObject payload = parsedMessage->payload.as<JsonObject>();
    TEST_ASSERT_EQUAL_STRING("ESP32", payload["chargePointModel"]);
    TEST_ASSERT_EQUAL_STRING("Test", payload["chargePointVendor"]);
}

void test_parse_call_result_message() {
    std::string callResultMessage = R"([3,"12345",{"status":"Accepted","interval":300}])";

    auto parsedMessage = OCPPMessageParser::parseMessage(callResultMessage);

    TEST_ASSERT_NOT_NULL(parsedMessage.get());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MessageType::CALLRESULT), static_cast<int>(parsedMessage->messageType));
    TEST_ASSERT_EQUAL_STRING("12345", parsedMessage->messageId.c_str());

    JsonObject result = parsedMessage->result.as<JsonObject>();
    TEST_ASSERT_EQUAL_STRING("Accepted", result["status"]);
    TEST_ASSERT_EQUAL_INT(300, result["interval"]);
}

void test_parse_call_error_message() {
    std::string callErrorMessage = R"([4,"12345","NotSupported","Request not supported",{}])";

    auto parsedMessage = OCPPMessageParser::parseMessage(callErrorMessage);

    TEST_ASSERT_NOT_NULL(parsedMessage.get());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MessageType::CALLERROR), static_cast<int>(parsedMessage->messageType));
    TEST_ASSERT_EQUAL_STRING("12345", parsedMessage->messageId.c_str());
    TEST_ASSERT_EQUAL_STRING("NotSupported", parsedMessage->errorCode.c_str());
    TEST_ASSERT_EQUAL_STRING("Request not supported", parsedMessage->errorDescription.c_str());
}

void test_validate_message_call_ok() {
    std::string callMessage = R"([2,"12345","BootNotification",{"chargePointModel":"ESP32","chargePointVendor":"Test"}])";

    auto parsedMessage = OCPPMessageParser::parseMessage(callMessage);

    TEST_ASSERT_NOT_NULL(parsedMessage.get());
    TEST_ASSERT_TRUE(OCPPMessageParser::validateMessage(*parsedMessage));
}

void test_validate_message_missing_action() {
    OCPPMessage message(MessageType::CALL, "1");
    message.payload.to<JsonObject>();

    TEST_ASSERT_FALSE(OCPPMessageParser::validateMessage(message));
}

void test_serialize_call_message() {
    JsonDocument payload;
    payload["chargePointModel"] = "ESP32";
    payload["chargePointVendor"] = "Test";

    std::string serialized = OCPPMessageParser::serializeCall("12345", "BootNotification", payload.as<JsonObject>());

    TEST_ASSERT_TRUE(serialized.find("[2,") == 0);
    TEST_ASSERT_TRUE(serialized.find("\"12345\"") != std::string::npos);
    TEST_ASSERT_TRUE(serialized.find("\"BootNotification\"") != std::string::npos);
    TEST_ASSERT_TRUE(serialized.find("\"ESP32\"") != std::string::npos);
}

void test_serialize_call_result_message() {
    JsonDocument result;
    result["status"] = "Accepted";
    result["interval"] = 300;

    std::string serialized = OCPPMessageParser::serializeCallResult("12345", result.as<JsonObject>());

    TEST_ASSERT_TRUE(serialized.find("[3,") == 0);
    TEST_ASSERT_TRUE(serialized.find("\"12345\"") != std::string::npos);
    TEST_ASSERT_TRUE(serialized.find("\"Accepted\"") != std::string::npos);
}

void test_websocket_connection_success() {
    SecurityConfig config;
    config.profile = SecurityProfile::Profile1_NoSecurity;

    bool connected = mockWebSocket->connect("ws://test-server:8080/ocpp", config);

    TEST_ASSERT_TRUE(connected);
    TEST_ASSERT_TRUE(mockWebSocket->isConnected());
    TEST_ASSERT_EQUAL_STRING("ws://test-server:8080/ocpp", mockWebSocket->getLastConnectedUrl().c_str());
}

void test_websocket_send_message() {
    SecurityConfig config;
    mockWebSocket->connect("ws://test-server:8080/ocpp", config);

    bool sent = mockWebSocket->sendMessage("test message");

    TEST_ASSERT_TRUE(sent);
    TEST_ASSERT_TRUE(mockWebSocket->hasOutgoingMessage());
    TEST_ASSERT_EQUAL_STRING("test message", mockWebSocket->getNextOutgoingMessage().c_str());
}

void test_config_repository_save_load() {
    MockFileSystem fs;
    LittleFSConfigRepository repo(&fs, "");

    Core::Domain::Configuration config;
    config.centralSystemUrl = "ws://csms.local/ocpp";
    config.chargePointId = "CP_TEST";
    config.heartbeatInterval = 120;
    config.numberOfConnectors = 2;
    config.authorizeRemoteTxRequests = false;

    TEST_ASSERT_TRUE(repo.saveConfiguration(config));

    auto loaded = repo.loadConfiguration();
    TEST_ASSERT_EQUAL_STRING("ws://csms.local/ocpp", loaded.centralSystemUrl.c_str());
    TEST_ASSERT_EQUAL_STRING("CP_TEST", loaded.chargePointId.c_str());
    TEST_ASSERT_EQUAL_INT(120, loaded.heartbeatInterval);
    TEST_ASSERT_EQUAL_INT(2, loaded.numberOfConnectors);
    TEST_ASSERT_FALSE(loaded.authorizeRemoteTxRequests);
}

void test_transaction_repository_roundtrip() {
    MockFileSystem fs;
    LittleFSTransactionRepository repo(&fs, "");

    Core::Domain::Transaction tx(1, "RFID1234");
    tx.transactionId = 42;
    tx.startMeterValue = 100;
    tx.isActive = true;
    tx.startTime = std::chrono::system_clock::time_point(std::chrono::seconds(10));

    TEST_ASSERT_TRUE(repo.saveTransaction(tx));

    auto loaded = repo.getTransaction(42);
    TEST_ASSERT_TRUE(loaded.has_value());
    TEST_ASSERT_EQUAL_INT(42, loaded->transactionId);
    TEST_ASSERT_EQUAL_INT(1, loaded->connectorId);
    TEST_ASSERT_EQUAL_STRING("RFID1234", loaded->idTag.c_str());
    TEST_ASSERT_TRUE(loaded->isActive);

    auto active = repo.getActiveTransactions();
    TEST_ASSERT_EQUAL_INT(1, active.size());
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_parse_call_message);
    RUN_TEST(test_parse_call_result_message);
    RUN_TEST(test_parse_call_error_message);
    RUN_TEST(test_validate_message_call_ok);
    RUN_TEST(test_validate_message_missing_action);
    RUN_TEST(test_serialize_call_message);
    RUN_TEST(test_serialize_call_result_message);
    RUN_TEST(test_websocket_connection_success);
    RUN_TEST(test_websocket_send_message);
    RUN_TEST(test_config_repository_save_load);
    RUN_TEST(test_transaction_repository_roundtrip);
    UNITY_END();
}

void loop() {}
