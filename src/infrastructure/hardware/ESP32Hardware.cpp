#include "ESP32Hardware.h"
#include <Arduino.h>

using namespace Infrastructure;

// Static instance for interrupt handling
ESP32Hardware* ESP32Hardware::instance = nullptr;

// =============================================================================
// UARTRFIDReader Implementation
// =============================================================================

UARTRFIDReader::UARTRFIDReader(int rx, int tx) : rxPin(rx), txPin(tx) {
#if defined(CONFIG_IDF_TARGET_ESP32C3)
    serial = &Serial1;
#else
    serial = &Serial2;
#endif
}

UARTRFIDReader::~UARTRFIDReader() {
    if (initialized) {
        serial->end();
    }
}

bool UARTRFIDReader::initialize() {
    if (initialized) return true;
    
    serial->begin(9600, SERIAL_8N1, rxPin, txPin);
    delay(100); // Allow hardware to settle
    
    // Test communication
    serial->flush();
    initialized = true;
    
    Serial.println("UART RFID Reader initialized");
    return true;
}

std::string UARTRFIDReader::readTag() {
    if (!initialized || !serial->available()) {
        return "";
    }
    
    String tagData = serial->readStringUntil('\n');
    tagData.trim();
    
    if (tagData.length() > 0) {
        std::string parsedTag = parseTagData(tagData.c_str());
        if (!parsedTag.empty()) {
            Serial.printf("RFID Tag read: %s\n", parsedTag.c_str());
            
            // Call callback if set
            if (tagCallback) {
                tagCallback(parsedTag);
            }
            
            return parsedTag;
        }
    }
    
    return "";
}

bool UARTRFIDReader::isTagPresent() {
    return initialized && serial->available() > 0;
}

void UARTRFIDReader::setTagCallback(std::function<void(const std::string&)> callback) {
    tagCallback = callback;
}

std::string UARTRFIDReader::parseTagData(const std::string& rawData) {
    // Simple parser - assumes hex format
    // Real implementation would depend on specific RFID reader format
    if (rawData.length() >= 8) {
        return rawData.substr(0, 8); // Return first 8 characters
    }
    return "";
}

void UARTRFIDReader::loop() {
    if (!initialized) return;
    
    // Check for new tag data
    if (isTagPresent()) {
        std::string tag = readTag();
        // Tag callback is called from readTag()
    }
}

// =============================================================================
// ADCCurrentSensor Implementation
// =============================================================================

ADCCurrentSensor::ADCCurrentSensor(int pin) : adcPin(pin) {
    // Initialize sample array
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        samples[i] = 0.0;
    }
}

bool ADCCurrentSensor::initialize() {
    if (initialized) return true;
    
    // Configure ADC
    analogSetAttenuation(ADC_11db); // For 0-3.3V range
    analogReadResolution(12); // 12-bit resolution
    
    // Take initial calibration reading
    calibrate();
    
    initialized = true;
    Serial.printf("ADC Current Sensor initialized on pin %d\n", adcPin);
    return true;
}

float ADCCurrentSensor::readCurrent() {
    if (!initialized) return 0.0;
    
    float voltage = readVoltage();
    return voltageToCurrent(voltage);
}

float ADCCurrentSensor::readVoltage() {
    if (!initialized) return 0.0;
    
    int adcValue = analogRead(adcPin);
    float voltage = adcToVoltage(adcValue);
    
    // Apply offset compensation
    voltage -= offsetVoltage;
    
    // Add to sample buffer for filtering
    samples[sampleIndex] = voltage;
    sampleIndex = (sampleIndex + 1) % SAMPLE_COUNT;
    if (sampleIndex == 0) samplesReady = true;
    
    return getFilteredReading();
}

float ADCCurrentSensor::readPower() {
    // Assuming 230V nominal voltage for power calculation
    // Real implementation would measure actual voltage
    float current = readCurrent();
    return current * 230.0; // P = V * I
}

void ADCCurrentSensor::calibrate() {
    if (!initialized) return;
    
    // Take multiple readings for calibration
    float sum = 0.0;
    int readings = 100;
    
    for (int i = 0; i < readings; i++) {
        int adcValue = analogRead(adcPin);
        sum += adcToVoltage(adcValue);
        delay(10);
    }
    
    offsetVoltage = sum / readings;
    Serial.printf("Current sensor calibrated - Offset: %.3fV\n", offsetVoltage);
}

float ADCCurrentSensor::getFilteredReading() {
    if (!samplesReady) return samples[sampleIndex];
    
    float sum = 0.0;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        sum += samples[i];
    }
    return sum / SAMPLE_COUNT;
}

float ADCCurrentSensor::adcToVoltage(int adcValue) {
    // 12-bit ADC: 0-4095 maps to 0-3.3V
    return (adcValue / 4095.0) * 3.3;
}

float ADCCurrentSensor::voltageToCurrent(float voltage) {
    // Apply calibration factor
    // This depends on current sensor specifications
    return voltage * calibrationFactor;
}

// =============================================================================
// ESP32Hardware Implementation
// =============================================================================

ESP32Hardware::ESP32Hardware() {
    instance = this;
    emergencyStopPin = EMERGENCY_STOP_PIN;
}

ESP32Hardware::~ESP32Hardware() {
    if (monitoringTaskHandle) {
        vTaskDelete(monitoringTaskHandle);
    }
    instance = nullptr;
}

bool ESP32Hardware::initialize() {
    Serial.println("Initializing ESP32 Hardware...");
    
    // Initialize emergency stop pin
    pinMode(emergencyStopPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(emergencyStopPin), emergencyStopISR, FALLING);
    
    // Add default connector
    addConnector(1, RELAY_PIN_1, CURRENT_SENSOR_PIN, CONNECTOR_DETECT_PIN_1, STATUS_LED_PIN_1);
    
    // Initialize RFID reader
    rfidReader = std::make_unique<UARTRFIDReader>(RFID_RX_PIN, RFID_TX_PIN);
    if (!rfidReader->initialize()) {
        Serial.println("Failed to initialize RFID reader");
        rfidReader.reset();
    }
    
    // Initialize current sensor
    currentSensor = std::make_unique<ADCCurrentSensor>(CURRENT_SENSOR_PIN);
    if (!currentSensor->initialize()) {
        Serial.println("Failed to initialize current sensor");
        currentSensor.reset();
    }
    
    // Start monitoring task
    xTaskCreate(monitoringTask, "HardwareMonitor", 4096, this, 1, &monitoringTaskHandle);
    
    Serial.println("ESP32 Hardware initialized successfully");
    return true;
}

void ESP32Hardware::loop() {
    // Check RFID reader
    if (rfidReader) {
        auto reader = static_cast<UARTRFIDReader*>(rfidReader.get());
        reader->loop();
    }
    
    // Check emergency stop state changes
    bool currentEmergencyState = digitalRead(emergencyStopPin) == LOW;
    if (currentEmergencyState != emergencyStopPressed) {
        emergencyStopPressed = currentEmergencyState;
        if (emergencyStopPressed) {
            Serial.println("Emergency stop activated!");
            performEmergencyShutdown();
            if (emergencyCallback) {
                emergencyCallback();
            }
        }
    }
}

bool ESP32Hardware::enableCharging(int connectorId) {
    if (emergencyStopPressed) {
        Serial.println("Cannot enable charging - emergency stop active");
        return false;
    }
    
    auto pinIt = connectorPins.find(connectorId);
    if (pinIt == connectorPins.end()) {
        Serial.printf("Connector %d not configured\n", connectorId);
        return false;
    }
    
    if (!safetyCheckPassed(connectorId)) {
        Serial.printf("Safety check failed for connector %d\n", connectorId);
        return false;
    }
    
    // Enable relay
    digitalWrite(pinIt->second.relayPin, HIGH);
    chargingEnabled[connectorId] = true;
    
    // Update status LED
    updateStatusLED(connectorId, LEDStatus::CHARGING);
    
    Serial.printf("Charging enabled for connector %d\n", connectorId);
    return true;
}

bool ESP32Hardware::disableCharging(int connectorId) {
    auto pinIt = connectorPins.find(connectorId);
    if (pinIt == connectorPins.end()) {
        return false;
    }
    
    // Disable relay
    digitalWrite(pinIt->second.relayPin, LOW);
    chargingEnabled[connectorId] = false;
    
    // Update status LED
    updateStatusLED(connectorId, LEDStatus::AVAILABLE);
    
    Serial.printf("Charging disabled for connector %d\n", connectorId);
    return true;
}

float ESP32Hardware::getCurrentMeterValue(int connectorId) {
    if (!currentSensor) {
        return 0.0;
    }
    
    auto pinIt = connectorPins.find(connectorId);
    if (pinIt == connectorPins.end()) {
        return 0.0;
    }
    
    float current = currentSensor->readCurrent();
    lastMeterValues[connectorId] = current;
    return current;
}

bool ESP32Hardware::unlockConnector(int connectorId) {
    // For this implementation, unlocking means disabling charging
    // Real implementation might have solenoid locks
    Serial.printf("Unlocking connector %d\n", connectorId);
    return disableCharging(connectorId);
}

bool ESP32Hardware::isConnectorPlugged(int connectorId) {
    auto pinIt = connectorPins.find(connectorId);
    if (pinIt == connectorPins.end()) {
        return false;
    }
    
    bool plugged = digitalRead(pinIt->second.connectorDetectPin) == LOW;
    connectorPlugged[connectorId] = plugged;
    return plugged;
}

std::string ESP32Hardware::readRFIDTag() {
    if (!rfidReader) {
        return "";
    }
    
    return rfidReader->readTag();
}

void ESP32Hardware::setStatusLED(int connectorId, const std::string& status) {
    LEDStatus ledStatus = LEDStatus::OFF;
    
    if (status == "Available") {
        ledStatus = LEDStatus::AVAILABLE;
    } else if (status == "Preparing") {
        ledStatus = LEDStatus::PREPARING;
    } else if (status == "Charging") {
        ledStatus = LEDStatus::CHARGING;
    } else if (status == "SuspendedEV" || status == "SuspendedEVSE") {
        ledStatus = LEDStatus::PREPARING; // Blinking
    } else if (status == "Finishing") {
        ledStatus = LEDStatus::PREPARING; // Blinking
    } else if (status == "Reserved") {
        ledStatus = LEDStatus::PREPARING; // Blinking
    } else if (status == "Unavailable") {
        ledStatus = LEDStatus::UNAVAILABLE;
    } else if (status == "Faulted") {
        ledStatus = LEDStatus::ERROR;
    }
    
    updateStatusLED(connectorId, ledStatus);
}

bool ESP32Hardware::addConnector(int connectorId, int relayPin, int currentPin, 
                                int detectPin, int ledPin) {
    ConnectorPins pins;
    pins.relayPin = relayPin;
    pins.currentSensorPin = currentPin;
    pins.connectorDetectPin = detectPin;
    pins.statusLedPin = ledPin;
    
    // Configure pins
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW); // Initially off
    
    pinMode(detectPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(detectPin), connectorDetectISR, CHANGE);
    
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);
    
    connectorPins[connectorId] = pins;
    chargingEnabled[connectorId] = false;
    connectorPlugged[connectorId] = false;
    lastMeterValues[connectorId] = 0.0;
    
    Serial.printf("Connector %d added - Relay: %d, Current: %d, Detect: %d, LED: %d\n",
                  connectorId, relayPin, currentPin, detectPin, ledPin);
    
    return true;
}

void ESP32Hardware::removeConnector(int connectorId) {
    auto it = connectorPins.find(connectorId);
    if (it != connectorPins.end()) {
        // Disable charging first
        disableCharging(connectorId);
        
        // Remove pin configurations
        connectorPins.erase(it);
        chargingEnabled.erase(connectorId);
        connectorPlugged.erase(connectorId);
        lastMeterValues.erase(connectorId);
        
        Serial.printf("Connector %d removed\n", connectorId);
    }
}

std::vector<int> ESP32Hardware::getConnectorIds() {
    std::vector<int> ids;
    for (const auto& pair : connectorPins) {
        ids.push_back(pair.first);
    }
    return ids;
}

bool ESP32Hardware::safetyCheckPassed(int connectorId) {
    // Check if connector is properly plugged in
    if (!isConnectorPlugged(connectorId)) {
        return false;
    }
    
    // Check emergency stop
    if (emergencyStopPressed) {
        return false;
    }
    
    // Add more safety checks as needed
    return true;
}

void ESP32Hardware::performEmergencyShutdown() {
    Serial.println("Performing emergency shutdown!");
    
    // Disable all charging
    for (const auto& pair : connectorPins) {
        disableCharging(pair.first);
    }
    
    // Update all LEDs to error state
    for (const auto& pair : connectorPins) {
        updateStatusLED(pair.first, LEDStatus::ERROR);
    }
}

void ESP32Hardware::updateStatusLED(int connectorId, LEDStatus status) {
    auto pinIt = connectorPins.find(connectorId);
    if (pinIt == connectorPins.end()) {
        return;
    }
    
    int ledPin = pinIt->second.statusLedPin;
    
    switch (status) {
        case LEDStatus::OFF:
            digitalWrite(ledPin, LOW);
            break;
        case LEDStatus::AVAILABLE:
            digitalWrite(ledPin, HIGH); // Green solid
            break;
        case LEDStatus::PREPARING:
            // This would need PWM or timer for blinking
            digitalWrite(ledPin, HIGH);
            break;
        case LEDStatus::CHARGING:
            digitalWrite(ledPin, HIGH); // Blue solid
            break;
        case LEDStatus::ERROR:
            // This would need timer for blinking
            digitalWrite(ledPin, LOW);
            break;
        case LEDStatus::UNAVAILABLE:
            digitalWrite(ledPin, LOW); // Red solid
            break;
    }
}

void ESP32Hardware::setConnectorCallback(std::function<void(int, bool)> callback) {
    connectorCallback = callback;
}

void ESP32Hardware::setRFIDCallback(std::function<void(const std::string&)> callback) {
    rfidCallback = callback;
    if (rfidReader) {
        rfidReader->setTagCallback(callback);
    }
}

void ESP32Hardware::setEmergencyCallback(std::function<void()> callback) {
    emergencyCallback = callback;
}

float ESP32Hardware::getPower(int connectorId) {
    if (!currentSensor) return 0.0;
    return currentSensor->readPower();
}

float ESP32Hardware::getVoltage(int connectorId) {
    if (!currentSensor) return 0.0;
    return currentSensor->readVoltage();
}

void ESP32Hardware::resetEmergencyStop() {
    emergencyStopPressed = false;
    Serial.println("Emergency stop reset");
}

ESP32Hardware::HardwareStatus ESP32Hardware::getStatus() {
    HardwareStatus status;
    status.initialized = true;
    status.emergencyStop = emergencyStopPressed;
    status.relayStates = chargingEnabled;
    status.connectorStates = connectorPlugged;
    status.currentReadings = lastMeterValues;
    status.rfidReaderConnected = (rfidReader != nullptr);
    status.lastRfidTag = readRFIDTag();
    
    return status;
}

void ESP32Hardware::printDiagnostics() {
    Serial.println("=== Hardware Diagnostics ===");
    Serial.printf("Emergency Stop: %s\n", emergencyStopPressed ? "PRESSED" : "OK");
    Serial.printf("RFID Reader: %s\n", rfidReader ? "Connected" : "Disconnected");
    Serial.printf("Current Sensor: %s\n", currentSensor ? "Connected" : "Disconnected");
    
    for (const auto& pair : connectorPins) {
        int connectorId = pair.first;
        Serial.printf("Connector %d - Charging: %s, Plugged: %s, Current: %.2fA\n",
                      connectorId,
                      chargingEnabled[connectorId] ? "ON" : "OFF",
                      connectorPlugged[connectorId] ? "YES" : "NO",
                      lastMeterValues[connectorId]);
    }
    Serial.println("=== End Diagnostics ===");
}

// Static interrupt handlers
void IRAM_ATTR ESP32Hardware::emergencyStopISR() {
    if (instance) {
        // Emergency stop handling will be done in main loop
        // Just wake up the system here
    }
}

void IRAM_ATTR ESP32Hardware::connectorDetectISR() {
    if (instance) {
        // Connector state change handling in main loop
    }
}

void ESP32Hardware::monitoringTask(void* parameter) {
    ESP32Hardware* hardware = static_cast<ESP32Hardware*>(parameter);
    
    while (true) {
        // Monitor hardware health
        // Check for sensor disconnections, relay failures, etc.
        
        vTaskDelay(pdMS_TO_TICKS(5000)); // Check every 5 seconds
    }
}

bool ESP32Hardware::testRelay(int connectorId) {
    auto pinIt = connectorPins.find(connectorId);
    if (pinIt == connectorPins.end()) return false;
    
    Serial.printf("Testing relay for connector %d\n", connectorId);
    
    // Test on
    digitalWrite(pinIt->second.relayPin, HIGH);
    delay(100);
    
    // Test off
    digitalWrite(pinIt->second.relayPin, LOW);
    delay(100);
    
    return true;
}

bool ESP32Hardware::testCurrentSensor(int connectorId) {
    if (!currentSensor) return false;
    
    Serial.printf("Testing current sensor for connector %d\n", connectorId);
    float current = getCurrentMeterValue(connectorId);
    Serial.printf("Current reading: %.2fA\n", current);
    
    return true;
}

bool ESP32Hardware::testConnectorDetection(int connectorId) {
    auto pinIt = connectorPins.find(connectorId);
    if (pinIt == connectorPins.end()) return false;
    
    bool plugged = isConnectorPlugged(connectorId);
    Serial.printf("Connector %d detection test - Plugged: %s\n", 
                  connectorId, plugged ? "YES" : "NO");
    
    return true;
}

bool ESP32Hardware::testStatusLED(int connectorId) {
    auto pinIt = connectorPins.find(connectorId);
    if (pinIt == connectorPins.end()) return false;
    
    Serial.printf("Testing status LED for connector %d\n", connectorId);
    
    // Cycle through LED states
    updateStatusLED(connectorId, LEDStatus::OFF);
    delay(500);
    updateStatusLED(connectorId, LEDStatus::AVAILABLE);
    delay(500);
    updateStatusLED(connectorId, LEDStatus::CHARGING);
    delay(500);
    updateStatusLED(connectorId, LEDStatus::ERROR);
    delay(500);
    updateStatusLED(connectorId, LEDStatus::OFF);
    
    return true;
}

bool ESP32Hardware::testRFIDReader() {
    if (!rfidReader) return false;
    
    Serial.println("Testing RFID reader - present a tag");
    
    // Wait for tag
    unsigned long startTime = millis();
    while (millis() - startTime < 10000) { // 10 second timeout
        std::string tag = readRFIDTag();
        if (!tag.empty()) {
            Serial.printf("RFID test successful - Tag: %s\n", tag.c_str());
            return true;
        }
        delay(100);
    }
    
    Serial.println("RFID test timeout - no tag detected");
    return false;
}

void ESP32Hardware::enableLowPowerMode() {
    lowPowerMode = true;
    Serial.println("Low power mode enabled");
}

void ESP32Hardware::disableLowPowerMode() {
    lowPowerMode = false;
    Serial.println("Low power mode disabled");
}

void ESP32Hardware::calibrateMeter(int connectorId) {
    if (currentSensor) {
        currentSensor->calibrate();
    }
}

void ESP32Hardware::setMeterCalibration(int connectorId, float factor) {
    if (currentSensor) {
        auto sensor = static_cast<ADCCurrentSensor*>(currentSensor.get());
        sensor->setCalibrationFactor(factor);
    }
}
