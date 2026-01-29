#include "PresentationInterfaces.h"
#include <Arduino.h>

using namespace Presentation;
using namespace Application;

// Static constants
const char* WebInterface::ADMIN_USERNAME = "admin";
const char* WebInterface::ADMIN_PASSWORD = "admin123";

// =============================================================================
// WebInterface Implementation
// =============================================================================

WebInterface::WebInterface(ChargingStationApplication* app) : application(app) {
    server = new AsyncWebServer(WEB_SERVER_PORT);
    websocket = new AsyncWebSocket("/ws");
}

WebInterface::~WebInterface() {
    shutdown();
    delete server;
    delete websocket;
}

bool WebInterface::initialize() {
    Serial.println("Initializing Web Interface...");
    
    if (!application) {
        Serial.println("Application reference required");
        return false;
    }
    
    // Setup WebSocket
    websocket->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, 
                             AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->handleWebSocketMessage(server, client, type, arg, data, len);
    });
    server->addHandler(websocket);
    
    // Setup routes
    setupRoutes();
    
    // Start server
    server->begin();
    Serial.printf("Web server started on port %d\n", WEB_SERVER_PORT);
    
    return true;
}

void WebInterface::setupRoutes() {
    // Static content routes
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleRoot(request);
    });
    
    server->on("/dashboard", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleDashboard(request);
    });
    
    server->on("/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleConfiguration(request);
    });
    
    server->on("/config", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleConfigurationUpdate(request);
    });
    
    server->on("/transactions", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleTransactions(request);
    });
    
    server->on("/diagnostics", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleDiagnostics(request);
    });
    
    // API routes
    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleApiSystemStatus(request);
    });
    
    server->on("/api/connectors", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleApiConnectorStatus(request);
    });
    
    server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleApiConfiguration(request);
    });
    
    server->on("/api/statistics", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleApiStatistics(request);
    });
    
    // Transaction control
    server->on("/api/start", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleTransactionStart(request);
    });
    
    server->on("/api/stop", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleTransactionStop(request);
    });
    
    // CSS and JavaScript
    server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(200, "text/css", getCSS());
    });
    
    server->on("/script.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(200, "text/javascript", getJavaScript());
    });
    
    // 404 handler
    server->onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not Found");
    });
}

void WebInterface::handleRoot(AsyncWebServerRequest* request) {
    if (!authenticateRequest(request)) {
        handleAuthentication(request);
        return;
    }
    
    // Redirect to dashboard
    request->redirect("/dashboard");
}

void WebInterface::handleDashboard(AsyncWebServerRequest* request) {
    if (!authenticateRequest(request)) {
        handleAuthentication(request);
        return;
    }
    
    auto status = application->getOverallStatus();
    String html = generateDashboardHTML(status);
    request->send(200, "text/html", html);
}

void WebInterface::handleApiSystemStatus(AsyncWebServerRequest* request) {
    auto status = application->getOverallStatus();
    
    JsonDocument json;
    json["chargePointId"] = status.chargePointId;
    json["firmwareVersion"] = status.firmwareVersion;
    json["uptime"] = status.uptime;
    json["network"]["connected"] = status.network.wifiConnected;
    json["network"]["ipAddress"] = status.network.ipAddress;
    json["network"]["ocppConnected"] = status.network.ocppConnected;
    json["hardware"]["emergencyStop"] = status.hardware.emergencyStop;
    json["hardware"]["rfidConnected"] = status.hardware.rfidReaderConnected;
    
    JsonArray connectors = json["connectors"].to<JsonArray>();
    for (const auto& connector : status.connectors) {
        JsonObject connectorObj = connectors.add<JsonObject>();
        connectorObj["id"] = connector.connectorId;
        connectorObj["status"] = connector.status;
        connectorObj["available"] = connector.available;
        connectorObj["charging"] = connector.charging;
        connectorObj["transactionId"] = connector.currentTransactionId;
    }
    
    sendJsonResponse(request, json);
}

void WebInterface::handleTransactionStart(AsyncWebServerRequest* request) {
    if (!request->hasParam("idTag") || !request->hasParam("connectorId")) {
        sendErrorResponse(request, 400, "Missing required parameters");
        return;
    }
    
    String idTag = request->getParam("idTag")->value();
    int connectorId = request->getParam("connectorId")->value().toInt();
    
    auto chargingService = application->getChargingService();
    auto result = chargingService->startTransaction(idTag.c_str(), connectorId);
    
    if (result.success) {
        JsonDocument json;
        json["success"] = true;
        json["transactionId"] = result.data.transactionId;
        json["message"] = "Transaction started successfully";
        sendJsonResponse(request, json);
    } else {
        sendErrorResponse(request, 400, result.errorMessage.c_str());
    }
}

void WebInterface::handleTransactionStop(AsyncWebServerRequest* request) {
    if (!request->hasParam("transactionId")) {
        sendErrorResponse(request, 400, "Missing transaction ID");
        return;
    }
    
    int transactionId = request->getParam("transactionId")->value().toInt();
    String reason = request->hasParam("reason") ? 
                   request->getParam("reason")->value() : "Local";
    
    auto chargingService = application->getChargingService();
    auto result = chargingService->stopTransaction(transactionId, reason.c_str());
    
    if (result.success) {
        JsonDocument json;
        json["success"] = true;
        json["message"] = "Transaction stopped successfully";
        sendJsonResponse(request, json);
    } else {
        sendErrorResponse(request, 400, result.errorMessage.c_str());
    }
}

String WebInterface::generateDashboardHTML(const SystemStatus& status) {
    String html;
    html.reserve(4096);

    html += F("<!DOCTYPE html><html><head>");
    html += F("<title>ESP32 OCPP Charging Station</title>");
    html += F("<link rel=\"stylesheet\" href=\"/style.css\">");
    html += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    html += F("</head><body><div class=\"container\">");

    // Header
    html += F("<header><h1>ESP32 OCPP Charging Station</h1><div class=\"status-bar\">");
    html += F("<span class=\"status-item\">ID: ");
    html += status.chargePointId.c_str();
    html += F("</span>");
    html += F("<span class=\"status-item\">FW: ");
    html += status.firmwareVersion.c_str();
    html += F("</span>");
    html += F("<span class=\"status-item\">Uptime: ");
    html += status.uptime.c_str();
    html += F("</span></div></header>");

    // Navigation
    html += F("<nav>");
    html += F("<a href=\"/dashboard\" class=\"active\">Dashboard</a>");
    html += F("<a href=\"/config\">Configuration</a>");
    html += F("<a href=\"/transactions\">Transactions</a>");
    html += F("<a href=\"/diagnostics\">Diagnostics</a>");
    html += F("</nav>");

    // System status
    html += F("<main><div class=\"card-grid\"><div class=\"card\"><h3>System Status</h3><div class=\"status-grid\">");

    // Network
    html += F("<div class=\"status-item\"><label>Network:</label><span class=\"");
    html += status.network.wifiConnected ? F("connected") : F("disconnected");
    html += F("\">");
    html += status.network.wifiConnected ? F("Connected") : F("Disconnected");
    html += F("</span></div>");

    // OCPP
    html += F("<div class=\"status-item\"><label>OCPP:</label><span class=\"");
    html += status.network.ocppConnected ? F("connected") : F("disconnected");
    html += F("\">");
    html += status.network.ocppConnected ? F("Connected") : F("Disconnected");
    html += F("</span></div>");

    // Emergency stop
    html += F("<div class=\"status-item\"><label>Emergency Stop:</label><span class=\"");
    html += status.hardware.emergencyStop ? F("fault") : F("ok");
    html += F("\">");
    html += status.hardware.emergencyStop ? F("ACTIVE") : F("OK");
    html += F("</span></div>");

    html += F("</div></div>");

    // Connectors card
    html += F("<div class=\"card\"><h3>Connectors</h3><div class=\"connector-grid\">");

    for (const auto& connector : status.connectors) {
        html += F("<div class=\"connector-card\">");
        html += F("<h4>Connector ");
        html += String(connector.connectorId);
        html += F("</h4>");

        html += F("<div class=\"connector-status ");
        html += connector.status.c_str();
        html += F("\">");
        html += connector.status.c_str();
        html += F("</div>");

        html += F("<div class=\"connector-actions\">");
        html += F("<button onclick=\"startTransaction(");
        html += String(connector.connectorId);
        html += F(")\"");
        if (connector.charging) {
            html += F(" disabled");
        }
        html += F(">Start</button>");

        html += F("<button onclick=\"stopTransaction(");
        html += String(connector.currentTransactionId);
        html += F(")\"");
        if (!connector.charging) {
            html += F(" disabled");
        }
        html += F(">Stop</button>");
        html += F("</div></div>");
    }

    html += F("</div></div></div></main>");

    // Simple footer and script hook (actual JS served from /script.js)
    html += F("<script src=\"/script.js\"></script>");
    html += F("</div></body></html>");

    return html;
}

String WebInterface::getCSS() {
    return R"css(
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: Arial, sans-serif;
    background-color: #f5f5f5;
    color: #333;
}

.container {
    max-width: 1200px;
    margin: 0 auto;
    padding: 20px;
}

header {
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    color: white;
    padding: 20px;
    border-radius: 10px;
    margin-bottom: 20px;
}

header h1 {
    margin-bottom: 10px;
}

.status-bar {
    display: flex;
    gap: 20px;
    font-size: 0.9em;
}

nav {
    display: flex;
    gap: 10px;
    margin-bottom: 20px;
}

nav a {
    padding: 10px 20px;
    background: white;
    color: #667eea;
    text-decoration: none;
    border-radius: 5px;
    transition: all 0.3s ease;
}

nav a:hover,
nav a.active {
    background: #667eea;
    color: white;
}

.card-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 20px;
}

.card {
    background: white;
    padding: 20px;
    border-radius: 10px;
    box-shadow: 0 2px 10px rgba(0,0,0,0.1);
}

.card h3 {
    margin-bottom: 15px;
    color: #667eea;
}

.status-grid {
    display: grid;
    gap: 10px;
}

.status-item {
    display: flex;
    justify-content: space-between;
    padding: 5px 0;
    border-bottom: 1px solid #eee;
}

.status-item label {
    font-weight: bold;
}

.connected {
    color: #28a745;
}

.disconnected {
    color: #dc3545;
}

.fault {
    color: #dc3545;
    font-weight: bold;
}

.ok {
    color: #28a745;
}

.connector-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 15px;
}

.connector-card {
    border: 2px solid #eee;
    border-radius: 8px;
    padding: 15px;
    text-align: center;
}

.connector-status {
    padding: 8px;
    border-radius: 5px;
    margin: 10px 0;
    font-weight: bold;
}

.connector-status.Available {
    background: #d4edda;
    color: #155724;
}

.connector-status.Charging {
    background: #cce7ff;
    color: #004085;
}

.connector-status.Faulted {
    background: #f8d7da;
    color: #721c24;
}

.connector-actions {
    display: flex;
    gap: 10px;
    justify-content: center;
}

button {
    padding: 8px 16px;
    border: none;
    border-radius: 5px;
    cursor: pointer;
    font-weight: bold;
    transition: all 0.3s ease;
}

button:hover {
    transform: translateY(-1px);
}

button:disabled {
    opacity: 0.6;
    cursor: not-allowed;
    transform: none;
}

button:not(:disabled) {
    background: #667eea;
    color: white;
}

@media (max-width: 768px) {
    .container {
        padding: 10px;
    }
    
    .status-bar {
        flex-direction: column;
        gap: 5px;
    }
    
    nav {
        flex-wrap: wrap;
    }
    
    .connector-actions {
        flex-direction: column;
    }
}
)css";
}

String WebInterface::getJavaScript() {
    return R"js(
// Auto-refresh functionality
let autoRefreshEnabled = true;
let refreshInterval = 5000; // 5 seconds

function toggleAutoRefresh() {
    autoRefreshEnabled = !autoRefreshEnabled;
    if (autoRefreshEnabled) {
        startAutoRefresh();
    }
}

function startAutoRefresh() {
    if (autoRefreshEnabled) {
        setTimeout(() => {
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    updatePageWithStatus(data);
                    startAutoRefresh();
                })
                .catch(error => {
                    console.error('Error refreshing status:', error);
                    startAutoRefresh();
                });
        }, refreshInterval);
    }
}

function updatePageWithStatus(status) {
    // Update status indicators
    const networkStatus = document.querySelector('.network-status');
    if (networkStatus) {
        networkStatus.textContent = status.network.connected ? 'Connected' : 'Disconnected';
        networkStatus.className = status.network.connected ? 'connected' : 'disconnected';
    }
    
    // Update connector statuses
    status.connectors.forEach(connector => {
        const connectorCard = document.querySelector(`[data-connector="${connector.id}"]`);
        if (connectorCard) {
            const statusElement = connectorCard.querySelector('.connector-status');
            if (statusElement) {
                statusElement.textContent = connector.status;
                statusElement.className = `connector-status ${connector.status}`;
            }
        }
    });
}

// Initialize auto-refresh when page loads
document.addEventListener('DOMContentLoaded', function() {
    startAutoRefresh();
});

// Utility functions
function showNotification(message, type = 'info') {
    // Simple notification system
    const notification = document.createElement('div');
    notification.className = `notification ${type}`;
    notification.textContent = message;
    document.body.appendChild(notification);
    
    setTimeout(() => {
        notification.remove();
    }, 3000);
}

function confirmAction(message, callback) {
    if (confirm(message)) {
        callback();
    }
}
)js";
}

void WebInterface::sendJsonResponse(AsyncWebServerRequest* request, const JsonDocument& json) {
    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void WebInterface::sendErrorResponse(AsyncWebServerRequest* request, int code, const String& message) {
    JsonDocument json;
    json["success"] = false;
    json["error"] = message;
    sendJsonResponse(request, json);
}

void WebInterface::handleWebSocketMessage(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                                        AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client connected: %u\n", client->id());
            break;
            
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client disconnected: %u\n", client->id());
            break;
            
        case WS_EVT_DATA:
            // Handle incoming WebSocket data
            break;
            
        default:
            break;
    }
}

bool WebInterface::authenticateRequest(AsyncWebServerRequest* request) {
    // Simple basic authentication - in production use proper session management
    return request->authenticate(ADMIN_USERNAME, ADMIN_PASSWORD);
}

void WebInterface::handleAuthentication(AsyncWebServerRequest* request) {
    return request->requestAuthentication();
}

void WebInterface::shutdown() {
    if (server) {
        server->end();
        Serial.println("Web server stopped");
    }
}

void WebInterface::loop() {
    // Handle WebSocket cleanup and periodic updates
    websocket->cleanupClients();
}

void WebInterface::onSystemEvent(const SystemEvent& event) {
    // Broadcast system events to all connected WebSocket clients
    JsonDocument json;
    json["type"] = "system_event";
    json["event"]["type"] = static_cast<int>(event.type);
    json["event"]["message"] = event.message;
    json["event"]["timestamp"] = event.timestamp;
    
    for (const auto& data : event.data) {
        json["event"]["data"][data.first] = data.second;
    }
    
    String message;
    serializeJson(json, message);
    websocket->textAll(message);
}

// =============================================================================
// SerialInterface Implementation
// =============================================================================

SerialInterface::SerialInterface(ChargingStationApplication* app) : application(app) {
}

SerialInterface::~SerialInterface() {
}

bool SerialInterface::initialize() {
    Serial.println("\\n=== ESP32 OCPP Charging Station ===");
    Serial.println("Serial interface initialized");
    Serial.println("Type 'help' for available commands\\n");
    
    return true;
}

void SerialInterface::loop() {
    if (Serial.available()) {
        char c = Serial.read();
        
        if (c == '\n' || c == '\r') {
            if (currentCommand.length() > 0) {
                auto args = parseCommand(currentCommand);
                if (args.size() > 0) {
                    String command = args[0];
                    command.toLowerCase();
                    
                    if (command == "help") {
                        handleHelp();
                    } else if (command == "status") {
                        handleStatus();
                    } else if (command == "start") {
                        handleStart(args);
                    } else if (command == "stop") {
                        handleStop(args);
                    } else if (command == "config") {
                        handleConfig(args);
                    } else if (command == "diagnostics") {
                        handleDiagnostics();
                    } else if (command == "reset") {
                        handleReset();
                    } else if (command == "reboot") {
                        handleReboot();
                    } else if (command == "test") {
                        handleTest(args);
                    } else {
                        Serial.println("Unknown command. Type 'help' for available commands.");
                    }
                }
                currentCommand = "";
                Serial.print("> ");
            }
        } else if (c == '\b' || c == 127) { // Backspace
            if (currentCommand.length() > 0) {
                currentCommand.remove(currentCommand.length() - 1);
                if (echoEnabled) {
                    Serial.print("\\b \\b");
                }
            }
        } else if (c >= 32 && c <= 126) { // Printable characters
            currentCommand += c;
            if (echoEnabled) {
                Serial.print(c);
            }
        }
    }
}

void SerialInterface::handleHelp() {
    Serial.println("\\nAvailable Commands:");
    Serial.println("  help                    - Show this help message");
    Serial.println("  status                  - Show system status");
    Serial.println("  start <idTag> <conn>    - Start transaction");
    Serial.println("  stop <txId>             - Stop transaction");
    Serial.println("  config [key] [value]    - Show/set configuration");
    Serial.println("  diagnostics             - Show system diagnostics");
    Serial.println("  test [component]        - Test system components");
    Serial.println("  reset                   - Reset configuration");
    Serial.println("  reboot                  - Reboot system");
    Serial.println();
}

void SerialInterface::handleStatus() {
    auto status = application->getOverallStatus();
    printStatus(status);
}

void SerialInterface::handleStart(const std::vector<String>& args) {
    if (args.size() < 3) {
        Serial.println("Usage: start <idTag> <connectorId>");
        return;
    }
    
    String idTag = args[1];
    int connectorId = args[2].toInt();
    
    if (!validateConnectorId(connectorId)) {
        Serial.printf("Invalid connector ID: %d\\n", connectorId);
        return;
    }
    
    if (!validateIdTag(idTag)) {
        Serial.printf("Invalid ID tag: %s\\n", idTag.c_str());
        return;
    }
    
    auto chargingService = application->getChargingService();
    auto result = chargingService->startTransaction(idTag.c_str(), connectorId);
    
    if (result.success) {
        Serial.printf("Transaction started successfully - ID: %d\\n", result.data.transactionId);
    } else {
        Serial.printf("Failed to start transaction: %s\\n", result.errorMessage.c_str());
    }
}

void SerialInterface::handleStop(const std::vector<String>& args) {
    if (args.size() < 2) {
        Serial.println("Usage: stop <transactionId>");
        return;
    }
    
    int transactionId = args[1].toInt();
    
    auto chargingService = application->getChargingService();
    auto result = chargingService->stopTransaction(transactionId, "Local");
    
    if (result.success) {
        Serial.printf("Transaction %d stopped successfully\\n", transactionId);
    } else {
        Serial.printf("Failed to stop transaction: %s\\n", result.errorMessage.c_str());
    }
}

void SerialInterface::printStatus(const SystemStatus& status) {
    Serial.println("\\n=== System Status ===");
    Serial.printf("Charge Point ID: %s\\n", status.chargePointId.c_str());
    Serial.printf("Firmware Version: %s\\n", status.firmwareVersion.c_str());
    Serial.printf("Uptime: %s\\n", status.uptime.c_str());
    Serial.printf("Network: %s\\n", status.network.wifiConnected ? "Connected" : "Disconnected");
    Serial.printf("OCPP: %s\\n", status.network.ocppConnected ? "Connected" : "Disconnected");
    Serial.printf("Emergency Stop: %s\\n", status.hardware.emergencyStop ? "ACTIVE" : "OK");
    
    Serial.println("\\nConnectors:");
    for (const auto& connector : status.connectors) {
        Serial.printf("  Connector %d: %s", connector.connectorId, connector.status.c_str());
        if (connector.charging) {
            Serial.printf(" (Transaction: %d)", connector.currentTransactionId);
        }
        Serial.println();
    }
    Serial.println("=====================\\n");
}

std::vector<String> SerialInterface::parseCommand(const String& input) {
    std::vector<String> tokens;
    int start = 0;
    int end = 0;
    
    while (end < input.length()) {
        while (end < input.length() && input.charAt(end) != ' ') {
            end++;
        }
        
        if (end > start) {
            tokens.push_back(input.substring(start, end));
        }
        
        while (end < input.length() && input.charAt(end) == ' ') {
            end++;
        }
        
        start = end;
    }
    
    return tokens;
}

void SerialInterface::onSystemEvent(const SystemEvent& event) {
    Serial.printf("[EVENT] %s: %s\\n", 
                 std::to_string(static_cast<int>(event.type)).c_str(),
                 event.message.c_str());
}

// =============================================================================
// PresentationManager Implementation
// =============================================================================

PresentationManager::PresentationManager(ChargingStationApplication* app) 
    : application(app) {
}

PresentationManager::~PresentationManager() {
    shutdown();
}

bool PresentationManager::initialize() {
    Serial.println("Initializing Presentation Manager...");
    
    // Initialize serial interface
    if (serialEnabled) {
        serialInterface = std::make_unique<SerialInterface>(application);
        if (!serialInterface->initialize()) {
            Serial.println("Failed to initialize serial interface");
            return false;
        }
    }
    
    // Initialize web interface
    if (webEnabled) {
        webInterface = std::make_unique<WebInterface>(application);
        if (!webInterface->initialize()) {
            Serial.println("Failed to initialize web interface");
            return false;
        }
    }
    
    // Initialize LED interface
    if (ledEnabled) {
        ledInterface = std::make_unique<LEDInterface>(application);
        if (!ledInterface->initialize()) {
            Serial.println("Failed to initialize LED interface");
            return false;
        }
    }
    
    // Subscribe to system events
    subscribeToEvents();
    
    Serial.println("Presentation Manager initialized successfully");
    return true;
}

void PresentationManager::shutdown() {
    unsubscribeFromEvents();
    
    if (webInterface) {
        webInterface->shutdown();
    }
    
    Serial.println("Presentation Manager shutdown complete");
}

void PresentationManager::loop() {
    if (serialInterface) {
        serialInterface->loop();
    }
    
    if (webInterface) {
        webInterface->loop();
    }
    
    if (ledInterface) {
        ledInterface->loop();
    }
}

void PresentationManager::subscribeToEvents() {
    if (!eventsSubscribed && application) {
        auto chargingService = application->getChargingService();
        if (chargingService) {
            chargingService->subscribeToEvents([this](const SystemEvent& event) {
                this->handleSystemEvent(event);
            });
            eventsSubscribed = true;
        }
    }
}

void PresentationManager::unsubscribeFromEvents() {
    if (eventsSubscribed && application) {
        auto chargingService = application->getChargingService();
        if (chargingService) {
            chargingService->unsubscribeFromEvents();
            eventsSubscribed = false;
        }
    }
}

void PresentationManager::handleSystemEvent(const SystemEvent& event) {
    // Forward event to all presentation interfaces
    if (serialInterface) {
        serialInterface->onSystemEvent(event);
    }
    
    if (webInterface) {
        webInterface->onSystemEvent(event);
    }
    
    if (ledInterface) {
        ledInterface->onSystemEvent(event);
    }
}

std::unique_ptr<PresentationManager> PresentationManager::create(ChargingStationApplication* app) {
    auto manager = std::make_unique<PresentationManager>(app);
    
    if (!manager->initialize()) {
        Serial.println("Failed to create presentation manager");
        return nullptr;
    }
    
    return manager;
}

// =============================================================================
// LEDInterface Implementation (Simplified)
// =============================================================================

LEDInterface::LEDInterface(ChargingStationApplication* app) : application(app) {
    // Default LED configuration
    systemStatusLED = {18, false, true};  // GPIO 18
    networkStatusLED = {19, false, true}; // GPIO 19
    ocppStatusLED = {21, false, true};    // GPIO 21
}

LEDInterface::~LEDInterface() {
}

bool LEDInterface::initialize() {
    Serial.println("Initializing LED Interface...");
    
    // Configure system LEDs
    configureLED(systemStatusLED.pin, systemStatusLED.inverted);
    configureLED(networkStatusLED.pin, networkStatusLED.inverted);
    configureLED(ocppStatusLED.pin, ocppStatusLED.inverted);
    
    // Configure connector LEDs
    for (auto& connector : connectorLEDs) {
        configureLED(connector.second.pin, connector.second.inverted);
    }
    
    // Initial LED states
    setLED(systemStatusLED.pin, LEDState::SLOW_BLINK);
    setLED(networkStatusLED.pin, LEDState::OFF);
    setLED(ocppStatusLED.pin, LEDState::OFF);
    
    Serial.println("LED Interface initialized");
    return true;
}

void LEDInterface::configureLED(int pin, bool inverted) {
    pinMode(pin, OUTPUT);
    setLEDPhysical(pin, inverted ? HIGH : LOW); // OFF state
}

void LEDInterface::setLED(int pin, LEDState state) {
    currentStates[pin] = state;
    lastBlinkTime[pin] = millis();
    blinkState[pin] = false;
    
    // Handle immediate states
    switch (state) {
        case LEDState::OFF:
            setLEDPhysical(pin, LOW);
            break;
        case LEDState::ON:
            setLEDPhysical(pin, HIGH);
            break;
        default:
            // Blinking states handled in loop()
            break;
    }
}

void LEDInterface::setLEDPhysical(int pin, bool on) {
    auto configIt = connectorLEDs.find(pin);
    bool inverted = false;
    
    if (configIt != connectorLEDs.end()) {
        inverted = configIt->second.inverted;
    } else if (pin == systemStatusLED.pin) {
        inverted = systemStatusLED.inverted;
    } else if (pin == networkStatusLED.pin) {
        inverted = networkStatusLED.inverted;
    } else if (pin == ocppStatusLED.pin) {
        inverted = ocppStatusLED.inverted;
    }
    
    digitalWrite(pin, inverted ? !on : on);
}

void LEDInterface::loop() {
    processBlinking();
}

void LEDInterface::processBlinking() {
    unsigned long currentTime = millis();
    
    for (auto& state : currentStates) {
        int pin = state.first;
        LEDState ledState = state.second;
        
        unsigned long blinkInterval = 0;
        
        switch (ledState) {
            case LEDState::SLOW_BLINK:
                blinkInterval = 500; // 1 Hz
                break;
            case LEDState::FAST_BLINK:
                blinkInterval = 125; // 4 Hz
                break;
            case LEDState::PULSE:
                // Simple pulse implementation
                blinkInterval = 1000;
                break;
            default:
                continue; // Skip non-blinking states
        }
        
        if (currentTime - lastBlinkTime[pin] >= blinkInterval) {
            blinkState[pin] = !blinkState[pin];
            setLEDPhysical(pin, blinkState[pin]);
            lastBlinkTime[pin] = currentTime;
        }
    }
}

void LEDInterface::onSystemEvent(const SystemEvent& event) {
    switch (event.type) {
        case SystemEvent::Type::NETWORK_CONNECTED:
            setLED(networkStatusLED.pin, LEDState::ON);
            break;
        case SystemEvent::Type::NETWORK_DISCONNECTED:
            setLED(networkStatusLED.pin, LEDState::OFF);
            break;
        case SystemEvent::Type::OCPP_CONNECTED:
            setLED(ocppStatusLED.pin, LEDState::ON);
            break;
        case SystemEvent::Type::OCPP_DISCONNECTED:
            setLED(ocppStatusLED.pin, LEDState::SLOW_BLINK);
            break;
        case SystemEvent::Type::EMERGENCY_STOP_ACTIVATED:
            setAllLEDs(LEDState::FAST_BLINK);
            break;
        case SystemEvent::Type::EMERGENCY_STOP_CLEARED:
            // Restore normal LED states
            onSystemStatusUpdate(application->getOverallStatus());
            break;
        default:
            break;
    }
}

void LEDInterface::onSystemStatusUpdate(const SystemStatus& status) {
    // Update system status LED
    if (status.hardware.emergencyStop) {
        setLED(systemStatusLED.pin, LEDState::FAST_BLINK);
    } else {
        setLED(systemStatusLED.pin, LEDState::ON);
    }
    
    // Update network LED
    setLED(networkStatusLED.pin, status.network.wifiConnected ? LEDState::ON : LEDState::OFF);
    
    // Update OCPP LED
    if (status.network.ocppConnected) {
        setLED(ocppStatusLED.pin, LEDState::ON);
    } else if (status.network.wifiConnected) {
        setLED(ocppStatusLED.pin, LEDState::SLOW_BLINK);
    } else {
        setLED(ocppStatusLED.pin, LEDState::OFF);
    }
    
    // Update connector LEDs
    for (const auto& connector : status.connectors) {
        auto ledIt = connectorLEDs.find(connector.connectorId);
        if (ledIt != connectorLEDs.end()) {
            LEDState state = LEDState::OFF;
            
            if (connector.charging) {
                state = LEDState::ON;
            } else if (connector.available) {
                state = LEDState::SLOW_BLINK;
            }
            
            setLED(ledIt->second.pin, state);
        }
    }
}

void LEDInterface::setAllLEDs(LEDState state) {
    setLED(systemStatusLED.pin, state);
    setLED(networkStatusLED.pin, state);
    setLED(ocppStatusLED.pin, state);
    
    for (auto& connector : connectorLEDs) {
        setLED(connector.second.pin, state);
    }
}
