#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace Domain {
    
    /**
     * @brief OCPP Configuration entity
     */
    struct Configuration {
        std::string centralSystemUrl;
        std::string chargePointId;
        std::string chargePointPassword;
        int heartbeatInterval = 300;
        int meterValueSampleInterval = 60;
        int transactionMessageAttempts = 3;
        int transactionMessageRetryInterval = 60;
        bool authorizeRemoteTxRequests = true;
        bool localAuthorizeOffline = true;
        int numberOfConnectors = 1;
        std::string supportedFeatureProfiles = "Core,FirmwareManagement,LocalAuthListManagement";
        
        // Security Profile Settings (OCPP 1.6-J Section 6)
        int securityProfile = 1;  // 1=NoSecurity, 2=TLS, 3=TLS+ClientCert
        bool cpoName = false;     // CPO name verification
        
        // Certificate paths (stored in LittleFS)
        std::string caCertificatePath = "/certs/ca.pem";
        std::string clientCertificatePath = "/certs/client.pem"; 
        std::string clientPrivateKeyPath = "/certs/client.key";
        
        // Additional security settings
        bool verifyServerCertificate = true;
        bool verifyHostname = true;
        std::vector<std::string> allowedCipherSuites;
        uint32_t tlsHandshakeTimeout = 10000; // milliseconds
    };
    
    /**
     * @brief Charging Transaction entity
     */
    class Transaction {
    public:
        int transactionId;
        int connectorId;
        std::string idTag;
        std::chrono::system_clock::time_point startTime;
        std::chrono::system_clock::time_point stopTime;
        int startMeterValue;
        int stopMeterValue;
        std::string stopReason;
        bool isActive;
        
        Transaction(int connectorId, const std::string& idTag)
            : connectorId(connectorId), idTag(idTag), startTime(std::chrono::system_clock::now()),
              startMeterValue(0), stopMeterValue(0), isActive(true) {}
    };
    
    /**
     * @brief Connector entity representing a charging connector
     */
    class Connector {
    public:
        enum Status {
            Available,
            Preparing,
            Charging,
            SuspendedEV,
            SuspendedEVSE,
            Finishing,
            Reserved,
            Unavailable,
            Faulted
        };
        
        int connectorId;
        Status status;
        std::string errorCode;
        std::string info;
        std::chrono::system_clock::time_point timestamp;
        std::string vendorId;
        std::string vendorErrorCode;
        
        Connector(int id) : connectorId(id), status(Available), 
                           timestamp(std::chrono::system_clock::now()) {}
    };
    
    /**
     * @brief Meter Value entity for energy measurements
     */
    struct MeterValue {
        std::chrono::system_clock::time_point timestamp;
        std::vector<struct SampledValue> sampledValues;
    };
    
    struct SampledValue {
        std::string value;
        std::string context = "Sample.Periodic";
        std::string format = "Raw";
        std::string measurand = "Energy.Active.Import.Register";
        std::string phase;
        std::string location = "Outlet";
        std::string unit = "Wh";
    };
    
    /**
     * @brief Authorization cache entry
     */
    struct AuthorizationCacheEntry {
        std::string idTag;
        std::string status; // Accepted, Blocked, Expired, Invalid, ConcurrentTx
        std::chrono::system_clock::time_point expiryDate;
        std::string parentIdTag;
    };
}