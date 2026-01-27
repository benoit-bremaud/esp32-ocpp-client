# ESP32 OCPP Client Development Instructions

## Project Overview
This is an ESP32-based OCPP 1.6-J client implementation for electric vehicle charging stations. The project implements ALL OCPP 1.6-J features following CLEAN/SOLID architecture principles for maintainable, testable embedded code.

## Development Environment Setup

### Build System
- Use **PlatformIO** as the primary build system (`platformio.ini` configuration)
- Alternative: ESP-IDF native build system with `idf.py`
- Target platform: `espressif32` with framework `arduino` or `espidf`

### Key Dependencies
- **ArduinoWebsockets** or **ESP-IDF WebSocket**: For OCPP WebSocket communication
- **ArduinoJson**: JSON parsing/serialization for OCPP messages
- **WiFiManager**: WiFi configuration and connection management
- **SPIFFS/LittleFS**: File system for configuration storage
- **mbedTLS**: TLS/SSL for secure WebSocket connections

## Project Structure Conventions (CLEAN Architecture)

```
src/
├── main.cpp                    # Application entry point, dependency injection setup
├── domain/                     # Domain layer (business logic)
│   ├── entities/              # OCPP entities (Transaction, Connector, etc.)
│   ├── usecases/              # OCPP use cases (StartTransaction, etc.)
│   └── interfaces/            # Repository/service interfaces
├── infrastructure/            # Infrastructure layer (frameworks, external concerns)
│   ├── ocpp/                  # OCPP 1.6-J protocol implementation
│   │   ├── client/           # OCPPClient, MessageQueue, HeartbeatManager
│   │   ├── messages/         # All 52 OCPP 1.6-J message handlers
│   │   ├── schemas/          # JSON schema validation
│   │   └── websocket/        # WebSocket communication layer
│   ├── wifi/                  # WiFi connection management
│   ├── storage/              # SPIFFS/NVS persistence layer
│   ├── hardware/             # Hardware abstraction layer
│   └── time/                 # NTP time synchronization
├── application/              # Application layer (orchestration)
│   ├── services/             # Application services
│   ├── dto/                  # Data transfer objects
│   └── factories/            # Object creation factories
└── presentation/             # Presentation layer (UI, APIs)
    ├── web/                  # Web configuration interface
    ├── serial/               # Serial command interface
    └── led/                  # Status LED indicators
```

## Core Architecture Patterns

### SOLID Principles Implementation
- **Single Responsibility**: Each OCPP message handler handles only one message type
- **Open/Closed**: Use strategy pattern for different connector types and auth methods
- **Liskov Substitution**: Hardware interfaces must be substitutable (mock/real sensors)
- **Interface Segregation**: Separate interfaces for storage, communication, and hardware
- **Dependency Inversion**: Business logic depends on abstractions, not implementations

### OCPP 1.6-J Complete Feature Set
```cpp
// Core Profile (Required)
class CoreProfile {
    void handleAuthorize(const JsonObject& payload);
    void handleBootNotification(const JsonObject& payload);
    void handleChangeAvailability(const JsonObject& payload);
    void handleChangeConfiguration(const JsonObject& payload);
    void handleClearCache(const JsonObject& payload);
    void handleDataTransfer(const JsonObject& payload);
    void handleGetConfiguration(const JsonObject& payload);
    void handleHeartbeat(const JsonObject& payload);
    void handleMeterValues(const JsonObject& payload);
    void handleRemoteStartTransaction(const JsonObject& payload);
    void handleRemoteStopTransaction(const JsonObject& payload);
    void handleReset(const JsonObject& payload);
    void handleStartTransaction(const JsonObject& payload);
    void handleStatusNotification(const JsonObject& payload);
    void handleStopTransaction(const JsonObject& payload);
    void handleUnlockConnector(const JsonObject& payload);
};

// Firmware Management Profile (Optional)
class FirmwareProfile {
    void handleGetDiagnostics(const JsonObject& payload);
    void handleUpdateFirmware(const JsonObject& payload);
};

// Local Auth List Profile (Optional)
class LocalAuthProfile {
    void handleGetLocalListVersion(const JsonObject& payload);
    void handleSendLocalList(const JsonObject& payload);
};

// Reservation Profile (Optional)  
class ReservationProfile {
    void handleReserveNow(const JsonObject& payload);
    void handleCancelReservation(const JsonObject& payload);
};

// Smart Charging Profile (Optional)
class SmartChargingProfile {
    void handleSetChargingProfile(const JsonObject& payload);
    void handleClearChargingProfile(const JsonObject& payload);
    void handleGetCompositeSchedule(const JsonObject& payload);
};

// Remote Trigger Profile (Optional)
class RemoteTriggerProfile {
    void handleTriggerMessage(const JsonObject& payload);
};
```

### Dependency Injection Pattern
```cpp
// main.cpp setup
void setup() {
    // Infrastructure layer
    auto wifiManager = std::make_unique<WiFiManager>();
    auto storage = std::make_unique<SPIFFSStorage>();
    auto websocket = std::make_unique<ArduinoWebSocket>();
    auto hardware = std::make_unique<ESP32Hardware>();
    
    // Domain layer  
    auto transactionRepo = std::make_unique<TransactionRepository>(storage.get());
    auto configRepo = std::make_unique<ConfigRepository>(storage.get());
    
    // Application layer
    auto ocppClient = std::make_unique<OCPPClient>(
        websocket.get(), 
        configRepo.get(),
        hardware.get()
    );
    
    // Start application
    app = std::make_unique<ChargingStationApp>(
        std::move(ocppClient),
        std::move(wifiManager),
        std::move(hardware)
    );
}
```

## Development Workflows

### Build Commands
```bash
# PlatformIO build and upload
pio run -t upload
pio run -t uploadfs      # Upload filesystem
pio device monitor       # Serial monitoring

# ESP-IDF alternative
idf.py build
idf.py flash monitor
```

### Debugging Approach
- Use Serial output with different log levels (DEBUG, INFO, WARN, ERROR)
- Implement remote logging via OCPP DataTransfer messages
- Use ESP32's built-in debugging features with JTAG when available
- Monitor heap usage and task stack sizes

### Testing Strategy
- Use hardware-in-the-loop testing with actual OCPP central systems
- Implement mock WebSocket server for unit testing
- Test RFID authorization flows and charging sequences
- Validate power management and safety shutoff procedures

## OCPP-Specific Patterns

## OCPP-Specific Patterns

### Message Handler Architecture
```cpp
// Base message handler interface
class IMessageHandler {
public:
    virtual void handle(const std::string& messageId, const JsonObject& payload) = 0;
    virtual std::string getMessageType() const = 0;
};

// Specific handler implementation
class AuthorizeHandler : public IMessageHandler {
private:
    IAuthorizationService* authService;
    IConfigRepository* configRepo;
    
public:
    AuthorizeHandler(IAuthorizationService* auth, IConfigRepository* config)
        : authService(auth), configRepo(config) {}
    
    void handle(const std::string& messageId, const JsonObject& payload) override;
    std::string getMessageType() const override { return "Authorize"; }
};

// Message dispatcher
class MessageDispatcher {
private:
    std::map<std::string, std::unique_ptr<IMessageHandler>> handlers;
    
public:
    void registerHandler(std::unique_ptr<IMessageHandler> handler);
    void dispatch(const std::string& messageType, const std::string& messageId, 
                  const JsonObject& payload);
};
```

### State Machine Implementation
```cpp
class ChargingSessionStateMachine {
public:
    enum State { Available, Preparing, Charging, SuspendedEV, SuspendedEVSE, Finishing, Reserved, Unavailable, Faulted };
    
private:
    State currentState = Available;
    IHardwareController* hardware;
    ITransactionRepository* transactionRepo;
    
public:
    void transition(State newState, const std::string& reason);
    bool canTransition(State from, State to) const;
    void handleConnectorEvent(ConnectorEvent event);
};
```

### Repository Pattern for Data Persistence
```cpp
class ITransactionRepository {
public:
    virtual void save(const Transaction& transaction) = 0;
    virtual std::optional<Transaction> findById(int transactionId) = 0;
    virtual std::vector<Transaction> findActive() = 0;
    virtual void remove(int transactionId) = 0;
};

class SPIFFSTransactionRepository : public ITransactionRepository {
private:
    IFileSystem* fileSystem;
    
public:
    void save(const Transaction& transaction) override;
    std::optional<Transaction> findById(int transactionId) override;
    // ... implementation
};
```

## Security Considerations

- Implement certificate-based authentication for WebSocket TLS
- Validate all incoming OCPP messages against schemas
- Secure storage of sensitive configuration data
- Implement proper authorization token handling

## Key Configuration Files

- `platformio.ini`: Build configuration, dependencies, board settings
- `data/config.json`: OCPP and WiFi configuration
- `src/config.h`: Hardware pin definitions and constants
- `include/ocpp_config.h`: OCPP protocol configuration

## Common Pitfalls to Avoid

- Don't block the main loop with synchronous WebSocket operations
- Ensure proper memory management (avoid memory leaks in JSON handling)
- Handle WebSocket reconnection gracefully
- Implement watchdog timers for reliability
- Validate all external inputs (RFID, network messages)

## Hardware-Specific Notes

- Use ESP32's dual-core architecture: Network on Core 0, application logic on Core 1
- Implement proper power management for battery backup scenarios
- Use appropriate GPIO levels for relay control and sensor inputs
- Consider electromagnetic interference (EMI) in industrial environments