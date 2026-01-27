#pragma once\n\n// Hardware pin definitions for ESP32\n#define RELAY_PIN_CONNECTOR_1    2   // GPIO2 - Charging relay control\n#define CURRENT_SENSOR_PIN       34  // ADC1_CH6 - Current measurement\n#define RFID_RX_PIN             16   // UART2 RX\n#define RFID_TX_PIN             17   // UART2 TX\n#define STATUS_LED_PIN          18   // Status LED\n#define CONNECTOR_DETECT_PIN    19   // Connector plug detection\n#define EMERGENCY_STOP_PIN      21   // Emergency stop button\n\n// OCPP Configuration defaults\n#define DEFAULT_HEARTBEAT_INTERVAL          300   // 5 minutes\n#define DEFAULT_METER_VALUE_INTERVAL        60    // 1 minute\n#define DEFAULT_TRANSACTION_MESSAGE_ATTEMPTS 3\n#define DEFAULT_TRANSACTION_MESSAGE_RETRY    60    // 1 minute

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
#define V2G_CERT_FILE                       "/certs/v2g.pem"\n\n// WiFi Configuration\n#define WIFI_CONFIG_PORTAL_TIMEOUT          300   // 5 minutes\n#define WIFI_CONNECTION_TIMEOUT             30    // 30 seconds\n\n// WebSocket Configuration\n#define WEBSOCKET_RECONNECT_INTERVAL        10    // 10 seconds\n#define WEBSOCKET_PING_INTERVAL             30    // 30 seconds\n#define MESSAGE_TIMEOUT                     30    // 30 seconds\n\n// File paths\n#define CONFIG_FILE_PATH                    \"/config.json\"\n#define TRANSACTION_FILE_PATH               \"/transactions.json\"\n#define AUTH_CACHE_FILE_PATH                \"/auth_cache.json\"\n\n// Application version\n#define FIRMWARE_VERSION                    \"1.0.0\"\n#define OCPP_VERSION                        \"1.6\"\n#define CHARGE_POINT_VENDOR                 \"ESP32-OCPP\"\n#define CHARGE_POINT_MODEL                  \"ESP32-Dev\""