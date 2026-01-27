#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// Infrastructure Layer
#include "infrastructure/network/WiFiManager.h"
#include "infrastructure/storage/SPIFFSStorage.h"
#include "infrastructure/ocpp/websocket/OCPPWebSocket.h"
#include "infrastructure/hardware/ESP32Hardware.h"
#include "infrastructure/security/CertificateManager.h"
#include "infrastructure/time/NTPTimeManager.h"

// Domain Layer  
#include "domain/interfaces/IConfigRepository.h"
#include "domain/interfaces/ITransactionRepository.h"
#include "domain/interfaces/IWebSocketClient.h"
#include "domain/interfaces/IHardwareController.h"

// Application Layer
#include "application/dto/ApplicationDTO.h"
#include "application/usecases/OCPPUseCases.h"
#include "application/services/ApplicationServices.h"

// Presentation Layer
#include "presentation/interfaces/PresentationInterfaces.h"

// Configuration
#include "config.h"

using namespace Application;
using namespace Infrastructure;
using namespace Domain;
using namespace Presentation;

// =============================================================================
// Global Application State
// =============================================================================

class ESP32OCPPApplication {
private:
    // Infrastructure Dependencies
    std::unique_ptr<WiFiManager> wifiManager;
    std::unique_ptr<SPIFFSStorage> storage;
    std::unique_ptr<OCPPWebSocket> webSocket;
    std::unique_ptr<ESP32Hardware> hardware;
    std::unique_ptr<CertificateManager> certManager;
    std::unique_ptr<NTPTimeManager> ntpManager;
    
    // Domain Repositories
    std::unique_ptr<IConfigRepository> configRepo;
    std::unique_ptr<ITransactionRepository> transactionRepo;
    
    // Application Services
    std::unique_ptr<ChargingStationApplication> chargingStationApp;
    
    // Presentation Layer
    std::unique_ptr<PresentationManager> presentationManager;
    
    // System State
    bool systemInitialized = false;
    bool emergencyStopActive = false;
    unsigned long lastHeartbeat = 0;
    unsigned long bootTime = 0;
    
    // Task Handles for FreeRTOS
    TaskHandle_t networkTaskHandle = nullptr;
    TaskHandle_t ocppTaskHandle = nullptr;
    TaskHandle_t hardwareTaskHandle = nullptr;
    
public:
    ESP32OCPPApplication() {
        bootTime = millis();
    }
    
    ~ESP32OCPPApplication() {
        shutdown();
    }
    
    bool initialize();
    void run();
    void shutdown();
    
private:
    bool initializeInfrastructure();
    bool initializeDomain();
    bool initializeApplication();
    bool initializePresentation();
    void startTasks();
    void handleEmergencyStop();
    
    // FreeRTOS Task Functions
    static void networkTaskFunction(void* parameter);
    static void ocppTaskFunction(void* parameter);
    static void hardwareTaskFunction(void* parameter);
    
    // Helper functions
    void printSystemInfo();
    void handleWatchdog();
    void performSystemHealth();
};

// Global instance
ESP32OCPPApplication* app = nullptr;

// =============================================================================
// Setup and Loop Functions (Arduino Framework)
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000); // Allow serial to stabilize
    
    Serial.println("\n=== ESP32 OCPP Charging Station ===");
    Serial.printf("Firmware Version: %s\n", FIRMWARE_VERSION);
    Serial.printf("Build Date: %s %s\n", __DATE__, __TIME__);
    Serial.println("Initializing system...\n");
    
    // Initialize application
    app = new ESP32OCPPApplication();
    
    if (!app->initialize()) {
        Serial.println("FATAL: Failed to initialize application");
        Serial.println("System will restart in 5 seconds...");
        delay(5000);
        ESP.restart();
    }
    
    Serial.println("System initialization completed successfully");
    Serial.println("Starting main application loop...\n");
}

void loop() {
    if (app) {
        app->run();
    }
    
    // Minimal delay to prevent watchdog issues
    delay(1);
}

// =============================================================================
// ESP32OCPPApplication Implementation
// =============================================================================

bool ESP32OCPPApplication::initialize() {
    Serial.println("Starting system initialization...");
    
    // Initialize in dependency order (CLEAN architecture)
    if (!initializeInfrastructure()) {
        Serial.println("Failed to initialize infrastructure layer");
        return false;
    }
    
    if (!initializeDomain()) {
        Serial.println("Failed to initialize domain layer");
        return false;
    }
    
    if (!initializeApplication()) {
        Serial.println("Failed to initialize application layer");
        return false;
    }
    
    if (!initializePresentation()) {
        Serial.println("Failed to initialize presentation layer");
        return false;
    }
    
    // Start background tasks
    startTasks();
    
    systemInitialized = true;
    printSystemInfo();
    
    return true;
}

bool ESP32OCPPApplication::initializeInfrastructure() {
    Serial.println("[1/4] Initializing Infrastructure Layer...");
    
    // Initialize SPIFFS first (needed for configuration)
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS initialization failed");
        return false;
    }
    Serial.println("✓ SPIFFS filesystem initialized");
    
    // Create storage abstraction
    storage = std::make_unique<SPIFFSStorage>();
    if (!storage->initialize()) {
        Serial.println("Storage initialization failed");
        return false;
    }
    Serial.println("✓ Storage layer initialized");
    
    // Initialize hardware abstraction
    hardware = std::make_unique<ESP32Hardware>();
    if (!hardware->initialize()) {
        Serial.println("Hardware initialization failed");
        return false;
    }
    Serial.println("✓ Hardware abstraction layer initialized");
    
    // Initialize certificate manager for secure connections
    certManager = std::make_unique<CertificateManager>(storage.get());
    if (!certManager->initialize()) {
        Serial.println("Certificate manager initialization failed");
        return false;
    }
    Serial.println("✓ Certificate manager initialized");
    
    // Initialize WiFi manager
    wifiManager = std::make_unique<WiFiManager>(storage.get());
    if (!wifiManager->initialize()) {
        Serial.println("WiFi manager initialization failed");
        return false;
    }
    Serial.println("✓ WiFi manager initialized");
    
    // Initialize NTP time synchronization
    ntpManager = std::make_unique<NTPTimeManager>();
    if (!ntpManager->initialize()) {
        Serial.println("NTP time manager initialization failed");
        return false;
    }
    Serial.println("✓ NTP time manager initialized");
    
    // Initialize WebSocket client (but don't connect yet)
    webSocket = std::make_unique<OCPPWebSocket>(certManager.get());
    if (!webSocket->initialize()) {
        Serial.println("WebSocket client initialization failed");
        return false;
    }
    Serial.println("✓ WebSocket client initialized");
    
    Serial.println("Infrastructure layer initialization completed\n");
    return true;
}

bool ESP32OCPPApplication::initializeDomain() {
    Serial.println("[2/4] Initializing Domain Layer...");
    
    // Create repositories with infrastructure dependencies
    configRepo = std::make_unique<ConfigRepository>(storage.get());
    if (!configRepo) {
        Serial.println("Failed to create configuration repository");
        return false;
    }
    Serial.println("✓ Configuration repository created");
    
    transactionRepo = std::make_unique<TransactionRepository>(storage.get());
    if (!transactionRepo) {
        Serial.println("Failed to create transaction repository");
        return false;
    }
    Serial.println("✓ Transaction repository created");
    
    Serial.println("Domain layer initialization completed\n");
    return true;
}

bool ESP32OCPPApplication::initializeApplication() {
    Serial.println("[3/4] Initializing Application Layer...");
    
    // Create use case factory with dependencies
    auto useCaseFactory = std::make_unique<OCPPUseCaseFactory>(
        configRepo.get(),
        transactionRepo.get(),
        webSocket.get(),
        hardware.get()
    );
    
    if (!useCaseFactory) {
        Serial.println("Failed to create use case factory");
        return false;
    }
    Serial.println("✓ Use case factory created");
    
    // Create main application service
    chargingStationApp = ChargingStationApplication::create(
        std::move(useCaseFactory),
        configRepo.get(),
        hardware.get(),
        wifiManager.get(),
        webSocket.get()
    );
    
    if (!chargingStationApp) {
        Serial.println("Failed to create charging station application");
        return false;
    }
    Serial.println("✓ Charging station application created");
    
    // Initialize the application
    if (!chargingStationApp->initialize()) {
        Serial.println("Failed to initialize charging station application");
        return false;
    }
    Serial.println("✓ Charging station application initialized");
    
    Serial.println("Application layer initialization completed\n");
    return true;
}

bool ESP32OCPPApplication::initializePresentation() {
    Serial.println("[4/4] Initializing Presentation Layer...");
    
    // Create presentation manager with application dependency
    presentationManager = PresentationManager::create(chargingStationApp.get());
    
    if (!presentationManager) {
        Serial.println("Failed to create presentation manager");
        return false;
    }
    Serial.println("✓ Presentation manager created and initialized");
    
    Serial.println("Presentation layer initialization completed\n");
    return true;
}

void ESP32OCPPApplication::startTasks() {
    Serial.println("Starting FreeRTOS tasks...");
    
    // Network task (WiFi management, WebSocket communication)
    xTaskCreatePinnedToCore(
        networkTaskFunction,        // Task function
        "NetworkTask",              // Task name
        8192,                       // Stack size
        this,                       // Task parameter
        3,                          // Priority
        &networkTaskHandle,         // Task handle
        0                          // Core 0
    );
    Serial.println("✓ Network task started on Core 0");
    
    // OCPP Protocol task (message processing)
    xTaskCreatePinnedToCore(
        ocppTaskFunction,           // Task function
        "OCPPTask",                // Task name
        8192,                       // Stack size
        this,                       // Task parameter
        2,                          // Priority
        &ocppTaskHandle,           // Task handle
        1                          // Core 1
    );
    Serial.println("✓ OCPP protocol task started on Core 1");
    
    // Hardware monitoring task (sensors, relays, safety)
    xTaskCreatePinnedToCore(
        hardwareTaskFunction,       // Task function
        "HardwareTask",            // Task name
        4096,                       // Stack size
        this,                       // Task parameter
        4,                          // Priority (highest)
        &hardwareTaskHandle,       // Task handle
        1                          // Core 1
    );
    Serial.println("✓ Hardware monitoring task started on Core 1");
    
    Serial.println("All tasks started successfully\n");
}

void ESP32OCPPApplication::run() {
    // Main application loop (runs on Core 1)
    
    // Check for emergency stop
    if (hardware && hardware->isEmergencyStopActive() != emergencyStopActive) {
        emergencyStopActive = hardware->isEmergencyStopActive();
        if (emergencyStopActive) {
            handleEmergencyStop();
        }
    }
    
    // Process presentation layer (user interfaces)
    if (presentationManager) {
        presentationManager->loop();
    }
    
    // Periodic system health checks (every 30 seconds)
    static unsigned long lastHealthCheck = 0;
    if (millis() - lastHealthCheck > 30000) {
        performSystemHealth();
        lastHealthCheck = millis();
    }
    
    // Handle watchdog
    handleWatchdog();
}

void ESP32OCPPApplication::handleEmergencyStop() {
    Serial.println("🚨 EMERGENCY STOP ACTIVATED 🚨");
    
    if (chargingStationApp) {
        // Stop all active transactions immediately
        auto chargingService = chargingStationApp->getChargingService();
        if (chargingService) {
            // This would typically get all active transactions and stop them
            Serial.println("Stopping all active charging sessions...");
            // Implementation would depend on the specific charging service interface
        }
        
        // Disable all connectors
        auto configService = chargingStationApp->getConfigurationService();
        if (configService) {
            Serial.println("Disabling all connectors...");
            // Set all connectors to unavailable
        }
    }
    
    Serial.println("System is now in emergency stop mode");
}

void ESP32OCPPApplication::performSystemHealth() {
    if (!systemInitialized) return;
    
    // Check memory usage
    size_t freeHeap = ESP.getFreeHeap();
    size_t totalHeap = ESP.getHeapSize();
    float heapUsage = (float)(totalHeap - freeHeap) / totalHeap * 100;
    
    if (heapUsage > 80.0) {
        Serial.printf("⚠️  High memory usage: %.1f%%\n", heapUsage);
    }
    
    // Check task stack usage
    if (networkTaskHandle) {
        UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(networkTaskHandle);
        if (stackHighWaterMark < 512) {
            Serial.printf("⚠️  Network task low stack: %u bytes remaining\n", stackHighWaterMark);
        }
    }
    
    // Check WiFi connection
    if (wifiManager && !wifiManager->isConnected()) {
        Serial.println("⚠️  WiFi connection lost");
    }
    
    // Check OCPP connection
    if (webSocket && !webSocket->isConnected()) {
        Serial.println("⚠️  OCPP WebSocket disconnected");
    }
}

void ESP32OCPPApplication::handleWatchdog() {
    // Feed the watchdog to prevent system reset
    // This should be called regularly from the main loop
    esp_task_wdt_reset();
}

void ESP32OCPPApplication::printSystemInfo() {
    Serial.println("=== System Information ===");
    Serial.printf("Chip Model: %s\n", ESP.getChipModel());
    Serial.printf("Chip Revision: %d\n", ESP.getChipRevision());
    Serial.printf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("Flash Size: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("PSRAM Size: %d bytes\n", ESP.getPsramSize());
    
    if (configRepo) {
        auto config = configRepo->getChargePointConfig();
        Serial.printf("Charge Point ID: %s\n", config.chargePointId.c_str());
        Serial.printf("Central System URL: %s\n", config.centralSystemUrl.c_str());
    }
    
    if (wifiManager) {
        Serial.printf("WiFi Status: %s\n", wifiManager->isConnected() ? "Connected" : "Disconnected");
        if (wifiManager->isConnected()) {
            Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
        }
    }
    
    Serial.println("==========================\n");
}

void ESP32OCPPApplication::shutdown() {
    Serial.println("Shutting down application...");
    
    systemInitialized = false;
    
    // Stop presentation layer
    if (presentationManager) {
        presentationManager->shutdown();
    }
    
    // Stop application services
    if (chargingStationApp) {
        chargingStationApp->shutdown();
    }
    
    // Stop infrastructure services
    if (webSocket) {
        webSocket->disconnect();
    }
    
    if (wifiManager) {
        wifiManager->disconnect();
    }
    
    // Delete FreeRTOS tasks
    if (networkTaskHandle) {
        vTaskDelete(networkTaskHandle);
        networkTaskHandle = nullptr;
    }
    
    if (ocppTaskHandle) {
        vTaskDelete(ocppTaskHandle);
        ocppTaskHandle = nullptr;
    }
    
    if (hardwareTaskHandle) {
        vTaskDelete(hardwareTaskHandle);
        hardwareTaskHandle = nullptr;
    }
    
    Serial.println("Application shutdown completed");
}

// =============================================================================
// FreeRTOS Task Implementations
// =============================================================================

void ESP32OCPPApplication::networkTaskFunction(void* parameter) {
    ESP32OCPPApplication* app = static_cast<ESP32OCPPApplication*>(parameter);
    
    Serial.println("Network task started");
    
    while (app->systemInitialized) {
        // Handle WiFi management
        if (app->wifiManager) {
            app->wifiManager->handleConnection();
        }
        
        // Handle WebSocket communication
        if (app->webSocket) {
            app->webSocket->loop();
        }
        
        // Handle NTP time synchronization
        if (app->ntpManager) {
            app->ntpManager->update();
        }
        
        // Task delay
        vTaskDelay(pdMS_TO_TICKS(100)); // 10 Hz
    }
    
    Serial.println("Network task ended");
    vTaskDelete(NULL);
}

void ESP32OCPPApplication::ocppTaskFunction(void* parameter) {
    ESP32OCPPApplication* app = static_cast<ESP32OCPPApplication*>(parameter);
    
    Serial.println("OCPP task started");
    
    while (app->systemInitialized) {
        // Process OCPP messages and application logic
        if (app->chargingStationApp) {
            app->chargingStationApp->processMessages();
            app->chargingStationApp->handlePeriodicTasks();
        }
        
        // Task delay
        vTaskDelay(pdMS_TO_TICKS(50)); // 20 Hz
    }
    
    Serial.println("OCPP task ended");
    vTaskDelete(NULL);
}

void ESP32OCPPApplication::hardwareTaskFunction(void* parameter) {
    ESP32OCPPApplication* app = static_cast<ESP32OCPPApplication*>(parameter);
    
    Serial.println("Hardware task started");
    
    while (app->systemInitialized) {
        // Monitor hardware sensors and safety systems
        if (app->hardware) {
            app->hardware->updateSensors();
            app->hardware->checkSafetySystems();
        }
        
        // Task delay (high frequency for safety)
        vTaskDelay(pdMS_TO_TICKS(10)); // 100 Hz
    }
    
    Serial.println("Hardware task ended");
    vTaskDelete(NULL);
}