#pragma once

// Hardware pin definitions for ESP32
#define RELAY_PIN_CONNECTOR_1    2   // GPIO2 - Charging relay control
#define RELAY_PIN_1              2   // Alias for RELAY_PIN_CONNECTOR_1
#define CURRENT_SENSOR_PIN       34  // ADC1_CH6 - Current measurement
#define RFID_RX_PIN             16   // UART2 RX
#define RFID_TX_PIN             17   // UART2 TX
#define STATUS_LED_PIN          18   // Status LED
#define STATUS_LED_PIN_1        18   // Alias for STATUS_LED_PIN
#define CONNECTOR_DETECT_PIN    19   // Connector plug detection
#define CONNECTOR_DETECT_PIN_1  19   // Alias for CONNECTOR_DETECT_PIN
#define EMERGENCY_STOP_PIN      21   // Emergency stop button

// OCPP Configuration defaults
#define DEFAULT_HEARTBEAT_INTERVAL          300   // 5 minutes
#define DEFAULT_METER_VALUE_INTERVAL        60    // 1 minute
#define DEFAULT_TRANSACTION_MESSAGE_ATTEMPTS 3
#define DEFAULT_TRANSACTION_MESSAGE_RETRY    60    // 1 minute

// OCPP Security Profiles (1.6-J Specification)
#define DEFAULT_SECURITY_PROFILE            1     // 1=NoSecurity, 2=TLS, 3=TLS+ClientCert
#define SECURITY_PROFILE_NO_SECURITY        1     // Unsecured HTTP/WebSocket
#define SECURITY_PROFILE_TLS                2     // TLS with server certificate verification
#define SECURITY_PROFILE_TLS_CLIENT_CERT    3     // TLS with client certificate authentication

// Certificate storage paths
#define CERT_DIRECTORY                      "/certs"
#define CA_CERT_FILE                        "/certs/ca.pem"
#define CLIENT_CERT_FILE                    "/certs/client.pem"
#define CLIENT_KEY_FILE                     "/certs/client.key"
#define MFG_CA_CERT_FILE                    "/certs/mfg_ca.pem"
#define V2G_CA_CERT_FILE                    "/certs/v2g_ca.pem"
#define V2G_CERT_FILE                       "/certs/v2g.pem"

// WiFi Configuration
#define WIFI_CONFIG_PORTAL_TIMEOUT          300   // 5 minutes
#define WIFI_CONNECTION_TIMEOUT             30    // 30 seconds

// WebSocket Configuration
#define WEBSOCKET_RECONNECT_INTERVAL        10    // 10 seconds
#define WEBSOCKET_PING_INTERVAL             30    // 30 seconds
#define MESSAGE_TIMEOUT                     30    // 30 seconds

// File paths
#define CONFIG_FILE_PATH                    "/config.json"
#define TRANSACTION_FILE_PATH               "/transactions.json"
#define AUTH_CACHE_FILE_PATH                "/auth_cache.json"

// Application version
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION                    "1.0.0"
#endif
#ifndef OCPP_VERSION
#define OCPP_VERSION                        "1.6"
#endif
#ifndef CHARGE_POINT_VENDOR
#define CHARGE_POINT_VENDOR                 "ESP32-OCPP"
#endif
#ifndef CHARGE_POINT_MODEL
#define CHARGE_POINT_MODEL                  "ESP32-Dev"
#endif
#ifndef CHARGE_POINT_SERIAL
#define CHARGE_POINT_SERIAL                 "ESP32-001"
#endif
#ifndef CHARGE_BOX_SERIAL
#define CHARGE_BOX_SERIAL                   "ESP32-001"
#endif
