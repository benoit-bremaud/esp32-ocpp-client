#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>
#include <driver/adc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <memory>
#include <functional>
#include <map>
#include <vector>

#include "../../../core/domain/ports/IHardwareController.h"
#include "../../../include/config.h"

namespace Infrastructure {
    
    /**
     * @brief RFID reader interface for different RFID modules
     */
    class IRFIDReader {
    public:
        virtual ~IRFIDReader() = default;
        virtual bool initialize() = 0;
        virtual std::string readTag() = 0;
        virtual bool isTagPresent() = 0;
        virtual void setTagCallback(std::function<void(const std::string&)> callback) = 0;
    };
    
    /**
     * @brief Current sensor interface for different current measurement methods
     */
    class ICurrentSensor {
    public:
        virtual ~ICurrentSensor() = default;
        virtual bool initialize() = 0;
        virtual float readCurrent() = 0;
        virtual float readVoltage() = 0;
        virtual float readPower() = 0;
        virtual void calibrate() = 0;
    };
    
    /**
     * @brief Simple RFID reader implementation using UART
     */
    class UARTRFIDReader : public IRFIDReader {
    private:
        HardwareSerial* serial;
        int rxPin;
        int txPin;
        bool initialized = false;
        std::function<void(const std::string&)> tagCallback;
        
        std::string parseTagData(const std::string& rawData);
        
    public:
        UARTRFIDReader(int rx, int tx);
        ~UARTRFIDReader() override;
        
        bool initialize() override;
        std::string readTag() override;
        bool isTagPresent() override;
        void setTagCallback(std::function<void(const std::string&)> callback) override;
        
        void loop(); // Must be called in main loop
    };
    
    /**
     * @brief ADC-based current sensor implementation
     */
    class ADCCurrentSensor : public ICurrentSensor {
    private:
        int adcPin;
        float calibrationFactor = 1.0;
        float offsetVoltage = 0.0;
        bool initialized = false;
        
        // Moving average filter
        static const int SAMPLE_COUNT = 10;
        float samples[SAMPLE_COUNT];
        int sampleIndex = 0;
        bool samplesReady = false;
        
        float getFilteredReading();
        float adcToVoltage(int adcValue);
        float voltageToCurrent(float voltage);
        
    public:
        ADCCurrentSensor(int pin);
        ~ADCCurrentSensor() override = default;
        
        bool initialize() override;
        float readCurrent() override;
        float readVoltage() override;
        float readPower() override;
        void calibrate() override;
        
        // Configuration
        void setCalibrationFactor(float factor) { calibrationFactor = factor; }
        void setOffsetVoltage(float offset) { offsetVoltage = offset; }
    };
    
    /**
     * @brief ESP32 hardware controller implementation
     */
    class ESP32Hardware : public Core::Domain::IHardwareController {
    private:
        // Hardware components
        std::unique_ptr<IRFIDReader> rfidReader;
        std::unique_ptr<ICurrentSensor> currentSensor;
        
        // Pin configurations
        struct ConnectorPins {
            int relayPin;
            int currentSensorPin;
            int connectorDetectPin;
            int statusLedPin;
        };
        
        std::map<int, ConnectorPins> connectorPins;
        
        // Emergency stop
        int emergencyStopPin;
        bool emergencyStopPressed = false;
        
        // Status tracking
        std::map<int, bool> chargingEnabled;
        std::map<int, bool> connectorPlugged;
        std::map<int, float> lastMeterValues;
        
        // Interrupt handling
        static ESP32Hardware* instance;
        static void IRAM_ATTR emergencyStopISR();
        static void IRAM_ATTR connectorDetectISR();
        
        // Safety features
        bool safetyCheckPassed(int connectorId);
        void performEmergencyShutdown();
        
        // LED status control
        enum class LEDStatus {
            OFF,
            AVAILABLE,      // Green solid
            PREPARING,      // Blue blinking
            CHARGING,       // Blue solid
            ERROR,          // Red blinking
            UNAVAILABLE     // Red solid
        };
        
        void updateStatusLED(int connectorId, LEDStatus status);
        
        // Watchdog and monitoring
        TaskHandle_t monitoringTaskHandle = nullptr;
        static void monitoringTask(void* parameter);
        
    public:
        ESP32Hardware();
        ~ESP32Hardware() override;
        
        // Initialization
        bool initialize();
        void loop(); // Must be called in main loop
        
        // IHardwareController implementation
        bool enableCharging(int connectorId) override;
        bool disableCharging(int connectorId) override;
        float getCurrentMeterValue(int connectorId) override;
        bool unlockConnector(int connectorId) override;
        bool isConnectorPlugged(int connectorId) override;
        std::string readRFIDTag() override;
        void setStatusLED(int connectorId, const std::string& status) override;
        
        // Extended functionality
        
        // Connector management
        bool addConnector(int connectorId, int relayPin, int currentPin, 
                         int detectPin, int ledPin);
        void removeConnector(int connectorId);
        std::vector<int> getConnectorIds();
        
        // Current measurements
        float getPower(int connectorId);
        float getVoltage(int connectorId);
        
        // Safety features
        bool isEmergencyStopPressed() override { return emergencyStopPressed; }
        void resetEmergencyStop();
        
        // Calibration
        void calibrateMeter(int connectorId);
        void setMeterCalibration(int connectorId, float factor);
        
        // Diagnostics
        struct HardwareStatus {
            bool initialized;
            bool emergencyStop;
            std::map<int, bool> relayStates;
            std::map<int, bool> connectorStates;
            std::map<int, float> currentReadings;
            std::map<int, float> powerReadings;
            bool rfidReaderConnected;
            std::string lastRfidTag;
        };
        
        HardwareStatus getStatus();
        void printDiagnostics();
        
        // Hardware testing
        bool testRelay(int connectorId);
        bool testCurrentSensor(int connectorId);
        bool testConnectorDetection(int connectorId);
        bool testStatusLED(int connectorId);
        bool testRFIDReader();
        
        // Power management
        void enableLowPowerMode();
        void disableLowPowerMode();
        
        // Callbacks
        void setConnectorCallback(std::function<void(int, bool)> callback);
        void setRFIDCallback(std::function<void(const std::string&)> callback);
        void setEmergencyCallback(std::function<void()> callback);
        
    private:
        std::function<void(int, bool)> connectorCallback;
        std::function<void(const std::string&)> rfidCallback;
        std::function<void()> emergencyCallback;
        
        bool lowPowerMode = false;
    };
} // namespace Infrastructure
