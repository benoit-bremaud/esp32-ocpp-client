#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <map>
#include <memory>
#include "../../application/services/ApplicationServices.h"
#include "../../application/dto/ApplicationDTO.h"

namespace Presentation {
    
    /**
     * @brief Web-based user interface for charging station management
     * 
     * Following CLEAN Architecture principles:
     * - Presentation layer only handles UI concerns
     * - Depends on Application services for business operations
     * - Converts between web formats and application DTOs
     * - Single Responsibility: Only web UI concerns
     */
    class WebInterface {
    private:
        AsyncWebServer* server;
        Application::ChargingStationApplication* application;
        
        // Web server configuration (port value comes from build-time define WEB_SERVER_PORT)
        static const char* ADMIN_USERNAME;
        static const char* ADMIN_PASSWORD;
        
        // Authentication
        bool authenticateRequest(AsyncWebServerRequest* request);
        void handleAuthentication(AsyncWebServerRequest* request);
        
        // Route handlers - Dashboard
        void handleRoot(AsyncWebServerRequest* request);
        void handleDashboard(AsyncWebServerRequest* request);
        void handleSystemStatus(AsyncWebServerRequest* request);
        
        // Route handlers - Configuration
        void handleConfiguration(AsyncWebServerRequest* request);
        void handleConfigurationUpdate(AsyncWebServerRequest* request);
        void handleConfigurationReset(AsyncWebServerRequest* request);
        
        // Route handlers - Transactions
        void handleTransactions(AsyncWebServerRequest* request);
        void handleTransactionStart(AsyncWebServerRequest* request);
        void handleTransactionStop(AsyncWebServerRequest* request);
        void handleTransactionHistory(AsyncWebServerRequest* request);
        
        // Route handlers - Diagnostics
        void handleDiagnostics(AsyncWebServerRequest* request);
        void handleSystemTest(AsyncWebServerRequest* request);
        void handleLogs(AsyncWebServerRequest* request);
        
        // Route handlers - Security
        void handleSecurity(AsyncWebServerRequest* request);
        void handleCertificateUpload(AsyncWebServerRequest* request);
        void handleSecurityProfileChange(AsyncWebServerRequest* request);
        
        // API endpoints
        void handleApiSystemStatus(AsyncWebServerRequest* request);
        void handleApiConnectorStatus(AsyncWebServerRequest* request);
        void handleApiConfiguration(AsyncWebServerRequest* request);
        void handleApiStatistics(AsyncWebServerRequest* request);
        
        // Utility methods
        void setupRoutes();
        void setupWebSocketHandlers();
        String generateDashboardHTML(const Application::SystemStatus& status);
        String generateConfigurationHTML(const Application::SystemConfiguration& config);
        String generateTransactionsHTML(const std::vector<Domain::Transaction>& transactions);
        String generateDiagnosticsHTML();
        String generateSecurityHTML();
        
        // JSON response helpers
        void sendJsonResponse(AsyncWebServerRequest* request, const JsonDocument& json);
        void sendErrorResponse(AsyncWebServerRequest* request, int code, const String& message);
        void sendSuccessResponse(AsyncWebServerRequest* request, const String& message);
        
        // WebSocket for real-time updates
        AsyncWebSocket* websocket;
        void handleWebSocketMessage(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                                  AwsEventType type, void* arg, uint8_t* data, size_t len);
        void broadcastSystemUpdate();
        
        // Static content
        String getCSS();
        String getJavaScript();
        
    public:
        WebInterface(Application::ChargingStationApplication* app);
        ~WebInterface();
        
        bool initialize();
        void shutdown();
        void loop(); // For WebSocket and periodic updates
        
        // Configuration
        void setCredentials(const String& username, const String& password);
        void enableAuthentication(bool enable);
        
        // Status and control
        bool isRunning() const;
        int getConnectedClients() const;
        
        // Event handling
        void onSystemEvent(const Application::SystemEvent& event);
    };
    
    /**
     * @brief Serial command line interface
     */
    class SerialInterface {
    private:
        Application::ChargingStationApplication* application;
        
        // Command processing
        String currentCommand;
        bool echoEnabled = true;
        
        // Command handlers
        void handleHelp();
        void handleStatus();
        void handleStart(const std::vector<String>& args);
        void handleStop(const std::vector<String>& args);
        void handleConfig(const std::vector<String>& args);
        void handleDiagnostics();
        void handleReset();
        void handleReboot();
        void handleTest(const std::vector<String>& args);
        
        // Utility methods
        std::vector<String> parseCommand(const String& input);
        void printHelp();
        void printStatus(const Application::SystemStatus& status);
        void printConfiguration(const Application::SystemConfiguration& config);
        void printTransaction(const Domain::Transaction& transaction);
        
        // Command validation
        bool validateConnectorId(int connectorId);
        bool validateIdTag(const String& idTag);
        
    public:
        SerialInterface(Application::ChargingStationApplication* app);
        ~SerialInterface();
        
        bool initialize();
        void loop(); // Process serial input
        
        // Configuration
        void setEcho(bool enable) { echoEnabled = enable; }
        void setBaudRate(long baudRate);
        
        // Output methods
        void println(const String& message);
        void print(const String& message);
        void printf(const char* format, ...);
        
        // Event handling
        void onSystemEvent(const Application::SystemEvent& event);
    };
    
    /**
     * @brief LED status indicator management
     */
    class LEDInterface {
    private:
        Application::ChargingStationApplication* application;
        
        // LED configuration
        struct LEDConfig {
            int pin;
            bool inverted = false;
            bool enabled = true;
        };
        
        LEDConfig systemStatusLED;
        LEDConfig networkStatusLED;
        LEDConfig ocppStatusLED;
        std::map<int, LEDConfig> connectorLEDs;
        
        // LED states
        enum class LEDState {
            OFF,
            ON,
            SLOW_BLINK,   // 1 Hz
            FAST_BLINK,   // 4 Hz
            PULSE         // Breathing effect
        };
        
        std::map<int, LEDState> currentStates;
        std::map<int, unsigned long> lastBlinkTime;
        std::map<int, bool> blinkState;
        
        // Update methods
        void updateSystemStatusLED(const Application::SystemStatus& status);
        void updateNetworkStatusLED(bool connected);
        void updateOCPPStatusLED(bool connected);
        void updateConnectorLED(int connectorId, const Application::ConnectorStatus& status);
        
        // LED control
        void setLED(int pin, LEDState state);
        void setLEDPhysical(int pin, bool on);
        void processBlinking();
        
        // Configuration
        void configureLED(int pin, bool inverted = false);
        
    public:
        LEDInterface(Application::ChargingStationApplication* app);
        ~LEDInterface();
        
        bool initialize();
        void loop(); // Process LED updates and blinking
        
        // LED configuration
        void setSystemStatusLED(int pin, bool inverted = false);
        void setNetworkStatusLED(int pin, bool inverted = false);
        void setOCPPStatusLED(int pin, bool inverted = false);
        void setConnectorLED(int connectorId, int pin, bool inverted = false);
        
        // Control methods
        void enableLED(int pin, bool enable = true);
        void testLEDs(); // Test all LEDs
        void setAllLEDs(LEDState state); // Set all LEDs to same state
        
        // Event handling
        void onSystemEvent(const Application::SystemEvent& event);
        void onSystemStatusUpdate(const Application::SystemStatus& status);
    };
    
    /**
     * @brief Main presentation layer coordinator
     */
    class PresentationManager {
    private:
        Application::ChargingStationApplication* application;
        
        // Presentation interfaces
        std::unique_ptr<WebInterface> webInterface;
        std::unique_ptr<SerialInterface> serialInterface;
        std::unique_ptr<LEDInterface> ledInterface;
        
        // Configuration
        bool webEnabled = true;
        bool serialEnabled = true;
        bool ledEnabled = true;
        
        // Event subscription
        bool eventsSubscribed = false;
        
    public:
        PresentationManager(Application::ChargingStationApplication* app);
        ~PresentationManager();
        
        // Lifecycle
        bool initialize();
        void shutdown();
        void loop(); // Main presentation loop
        
        // Interface access
        WebInterface* getWebInterface() { return webInterface.get(); }
        SerialInterface* getSerialInterface() { return serialInterface.get(); }
        LEDInterface* getLEDInterface() { return ledInterface.get(); }
        
        // Configuration
        void enableWeb(bool enable) { webEnabled = enable; }
        void enableSerial(bool enable) { serialEnabled = enable; }
        void enableLED(bool enable) { ledEnabled = enable; }
        
        // Event handling
        void handleSystemEvent(const Application::SystemEvent& event);
        void subscribeToEvents();
        void unsubscribeFromEvents();
        
        // Factory method
        static std::unique_ptr<PresentationManager> create(
            Application::ChargingStationApplication* app);
    };
}