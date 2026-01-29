#pragma once

// Firmware information
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "v1.0.0-CLEAN"
#endif

// Network configuration
#ifndef WEB_SERVER_PORT
#define WEB_SERVER_PORT 80
#endif

#ifndef WEBSOCKET_PORT
#define WEBSOCKET_PORT 8080
#endif

#ifndef SERIAL_SPEED
#define SERIAL_SPEED 115200
#endif

// OCPP Configuration
#ifndef CHARGE_POINT_SERIAL
#define CHARGE_POINT_SERIAL "ESP32_OCPP_001"
#endif

#ifndef CHARGE_BOX_SERIAL
#define CHARGE_BOX_SERIAL "ESP32_CB_001"
#endif

#ifndef DEFAULT_HEARTBEAT_INTERVAL
#define DEFAULT_HEARTBEAT_INTERVAL 300
#endif

#ifndef MAX_PENDING_MESSAGES
#define MAX_PENDING_MESSAGES 100
#endif

// Charge point information
#ifndef CHARGE_POINT_VENDOR
#define CHARGE_POINT_VENDOR "ESP32_VENDOR"
#endif

#ifndef CHARGE_POINT_MODEL
#define CHARGE_POINT_MODEL "ESP32_OCPP_CLIENT"
#endif

#ifndef MESSAGE_TIMEOUT
#define MESSAGE_TIMEOUT 30
#endif

// Hardware pin definitions
namespace HardwarePins {
    // Status LEDs
    constexpr int SYSTEM_STATUS_LED = 2;    // Built-in LED on most ESP32 boards
    constexpr int NETWORK_STATUS_LED = 18;
    constexpr int OCPP_STATUS_LED = 19;
    
    // Connector LEDs (configurable for multiple connectors)
    constexpr int CONNECTOR_1_LED = 21;
    constexpr int CONNECTOR_2_LED = 22;
    
    // Safety systems
    constexpr int EMERGENCY_STOP_PIN = 23;
    constexpr int MAIN_CONTACTOR_PIN = 25;
    constexpr int GROUND_FAULT_PIN = 26;
    
    // RFID reader
    constexpr int RFID_SS_PIN = 5;
    constexpr int RFID_RST_PIN = 4;
    
    // Current/voltage sensors
    constexpr int CURRENT_SENSOR_1 = 32;
    constexpr int CURRENT_SENSOR_2 = 33;
    constexpr int VOLTAGE_SENSOR = 34;
    
    // Relay controls
    constexpr int RELAY_1_PIN = 16;
    constexpr int RELAY_2_PIN = 17;
}

// System limits and timeouts
namespace SystemLimits {
    constexpr unsigned long HEARTBEAT_INTERVAL_MS = 300000;  // 5 minutes
    constexpr unsigned long WEBSOCKET_RECONNECT_INTERVAL_MS = 30000;  // 30 seconds
    constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 30000;  // 30 seconds
    constexpr unsigned long TRANSACTION_TIMEOUT_MS = 3600000;  // 1 hour
    
    constexpr size_t MAX_MESSAGE_QUEUE_SIZE = 50;
    constexpr size_t MAX_TRANSACTION_HISTORY = 100;
    constexpr size_t JSON_BUFFER_SIZE = 2048;
    
    constexpr float MAX_CURRENT_AMPS = 32.0f;
    constexpr float MAX_VOLTAGE_VOLTS = 250.0f;
    constexpr float GROUND_FAULT_THRESHOLD_MA = 30.0f;
}

// OCPP configuration
namespace OCPPConfig {
    constexpr const char* DEFAULT_CHARGE_POINT_ID = "ESP32_OCPP_001";
    constexpr const char* DEFAULT_CENTRAL_SYSTEM_URL = "ws://localhost:8080/ocpp/ChargePointSimulator";
    constexpr const char* PROTOCOL_VERSION = "ocpp1.6";
    
    // Supported features
    constexpr bool SUPPORT_CORE_PROFILE = true;
    constexpr bool SUPPORT_FIRMWARE_PROFILE = true;
    constexpr bool SUPPORT_LOCAL_AUTH_PROFILE = true;
    constexpr bool SUPPORT_RESERVATION_PROFILE = true;
    constexpr bool SUPPORT_SMART_CHARGING_PROFILE = true;
    constexpr bool SUPPORT_REMOTE_TRIGGER_PROFILE = true;
    
    // Configuration keys
    constexpr const char* CONFIG_CHARGE_POINT_ID = "ChargePointId";
    constexpr const char* CONFIG_CENTRAL_SYSTEM_URL = "CentralSystemURL";
    constexpr const char* CONFIG_HEARTBEAT_INTERVAL = "HeartbeatInterval";
    constexpr const char* CONFIG_METER_VALUES_INTERVAL = "MeterValueSampleInterval";
    constexpr const char* CONFIG_CLOCK_ALIGNED_DATA_INTERVAL = "ClockAlignedDataInterval";
    constexpr const char* CONFIG_CONNECTION_TIMEOUT = "ConnectionTimeOut";
    constexpr const char* CONFIG_RESET_RETRIES = "ResetRetries";
    constexpr const char* CONFIG_CONNECTOR_PHASE_ROTATION = "ConnectorPhaseRotation";
    constexpr const char* CONFIG_MAX_ENERGY_ON_INVALID_ID = "MaxEnergyOnInvalidId";
    constexpr const char* CONFIG_SUPPORTED_FEATURE_PROFILES = "SupportedFeatureProfiles";
    constexpr const char* CONFIG_LOCAL_AUTHORIZE_OFFLINE = "LocalAuthorizeOffline";
    constexpr const char* CONFIG_LOCAL_PRE_AUTHORIZE = "LocalPreAuthorize";
}

// WiFi configuration
namespace WiFiConfig {
    constexpr const char* DEFAULT_AP_SSID = "ESP32_OCPP_Config";
    constexpr const char* DEFAULT_AP_PASSWORD = "configure123";
    constexpr const char* CONFIG_PORTAL_TIMEOUT_MS = "120000";  // 2 minutes
    
    // WiFi credentials storage keys
    constexpr const char* WIFI_SSID_KEY = "wifi_ssid";
    constexpr const char* WIFI_PASSWORD_KEY = "wifi_password";
}

// Storage paths
namespace StoragePaths {
    constexpr const char* CONFIG_FILE = "/config.json";
    constexpr const char* TRANSACTIONS_FILE = "/transactions.json";
    constexpr const char* CERTIFICATES_DIR = "/certs";
    constexpr const char* ROOT_CA_FILE = "/certs/root_ca.pem";
    constexpr const char* CLIENT_CERT_FILE = "/certs/client_cert.pem";
    constexpr const char* CLIENT_KEY_FILE = "/certs/client_key.pem";
    constexpr const char* LOGS_DIR = "/logs";
    constexpr const char* FIRMWARE_DIR = "/firmware";
}

// Security configuration
namespace SecurityConfig {
    constexpr int DEFAULT_SECURITY_PROFILE = 1;  // Unsecured Transport with Basic Authentication
    constexpr const char* DEFAULT_AUTH_USERNAME = "admin";
    constexpr const char* DEFAULT_AUTH_PASSWORD = "admin123";
    
    // Certificate validation settings
    constexpr bool VERIFY_SSL_CERTIFICATES = true;
    constexpr bool ALLOW_SELF_SIGNED_CERTS = false;
    constexpr unsigned long CERT_VALIDITY_CHECK_INTERVAL_MS = 86400000;  // 24 hours
}

// Debug and logging configuration
namespace DebugConfig {
    #ifdef DEBUG_ENABLED
    constexpr bool ENABLE_DEBUG_LOGS = true;
    #else
    constexpr bool ENABLE_DEBUG_LOGS = false;
    #endif
    
    constexpr bool ENABLE_SERIAL_DEBUG = true;
    constexpr bool ENABLE_WEB_DEBUG = true;
    constexpr bool ENABLE_REMOTE_LOGGING = false;
    
    // Log levels
    enum class LogLevel {
        ERROR = 0,
        WARN = 1,
        INFO = 2,
        DEBUG = 3,
        TRACE = 4
    };
    
    constexpr LogLevel DEFAULT_LOG_LEVEL = LogLevel::INFO;
    constexpr size_t MAX_LOG_ENTRIES = 1000;
    constexpr unsigned long LOG_ROTATION_INTERVAL_MS = 86400000;  // 24 hours
}

// Application configuration
namespace AppConfig {
    constexpr unsigned long SYSTEM_HEALTH_CHECK_INTERVAL_MS = 30000;  // 30 seconds
    constexpr unsigned long LED_UPDATE_INTERVAL_MS = 100;  // 100ms for smooth blinking
    constexpr unsigned long WEB_UI_REFRESH_INTERVAL_MS = 5000;  // 5 seconds
    constexpr unsigned long SERIAL_COMMAND_TIMEOUT_MS = 10000;  // 10 seconds
    
    // FreeRTOS task configuration
    constexpr size_t NETWORK_TASK_STACK_SIZE = 8192;
    constexpr UBaseType_t NETWORK_TASK_PRIORITY = 3;
    constexpr BaseType_t NETWORK_TASK_CORE = 0;
    
    constexpr size_t OCPP_TASK_STACK_SIZE = 8192;
    constexpr UBaseType_t OCPP_TASK_PRIORITY = 2;
    constexpr BaseType_t OCPP_TASK_CORE = 1;
    
    constexpr size_t HARDWARE_TASK_STACK_SIZE = 4096;
    constexpr UBaseType_t HARDWARE_TASK_PRIORITY = 4;  // Highest priority for safety
    constexpr BaseType_t HARDWARE_TASK_CORE = 1;
}

// Validation macros
#define VALIDATE_PIN(pin) ((pin) >= 0 && (pin) <= 39)
#define VALIDATE_ANALOG_PIN(pin) ((pin) >= 32 && (pin) <= 39)
#define VALIDATE_PWM_PIN(pin) ((pin) >= 0 && (pin) <= 33)

// Utility macros
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Compile-time assertions for critical configurations
static_assert(SystemLimits::MAX_MESSAGE_QUEUE_SIZE > 0, "Message queue size must be positive");
static_assert(SystemLimits::JSON_BUFFER_SIZE >= 512, "JSON buffer too small");
static_assert(AppConfig::NETWORK_TASK_STACK_SIZE >= 4096, "Network task stack too small");
static_assert(AppConfig::OCPP_TASK_STACK_SIZE >= 4096, "OCPP task stack too small");
static_assert(AppConfig::HARDWARE_TASK_STACK_SIZE >= 2048, "Hardware task stack too small");