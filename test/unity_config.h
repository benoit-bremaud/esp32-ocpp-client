#pragma once

/**
 * @brief Unity Test Configuration for ESP32 OCPP Client
 * 
 * This file configures the Unity testing framework for embedded testing
 * on the ESP32 platform with CLEAN architecture patterns.
 */

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <stdio.h>
#endif

// Unity configuration
#define UNITY_INCLUDE_DOUBLE
#define UNITY_INCLUDE_FLOAT
#define UNITY_SUPPORT_64

// Test output configuration
#ifdef ARDUINO
#define UNITY_OUTPUT_START()    Serial.begin(115200)
#define UNITY_OUTPUT_CHAR(c)    Serial.write(c)
#define UNITY_OUTPUT_FLUSH()    Serial.flush()
#define UNITY_OUTPUT_COMPLETE() Serial.println("\n--- Test Complete ---")
#else
#define UNITY_OUTPUT_START()    ((void)0)
#define UNITY_OUTPUT_CHAR(c)    putchar(c)
#define UNITY_OUTPUT_FLUSH()    fflush(stdout)
#define UNITY_OUTPUT_COMPLETE() printf("\n--- Test Complete ---\n")
#endif

// Memory management for embedded testing
#define UNITY_MAX_DETAILS       100
#define UNITY_MAX_EXPECTATIONS  20

// Custom assertions for ESP32/OCPP
#define TEST_ASSERT_OCPP_SUCCESS(actual) \
    TEST_ASSERT_TRUE_MESSAGE(actual, "OCPP operation failed")

#define TEST_ASSERT_VALID_JSON(jsonDoc) \
    TEST_ASSERT_FALSE_MESSAGE(jsonDoc.isNull(), "Invalid JSON document")

#define TEST_ASSERT_WEBSOCKET_CONNECTED(client) \
    TEST_ASSERT_TRUE_MESSAGE(client->isConnected(), "WebSocket not connected")

// Time-sensitive test helpers
#define TEST_TIMEOUT_MS 5000
#define TEST_DELAY_MS   100

// Mock configuration
#ifdef ENABLE_MOCK_HARDWARE
#define MOCK_ENABLED 1
#else
#define MOCK_ENABLED 0
#endif
