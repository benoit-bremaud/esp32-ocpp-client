#pragma once

#include <string>
#include <vector>

namespace Infrastructure {
    
    /**
     * @brief OCPP Security Profiles as defined in OCPP 1.6-J specification
     */
    enum class SecurityProfile {
        Profile1_NoSecurity = 1,    // Unsecured HTTP/WebSocket
        Profile2_TLS = 2,           // TLS with server certificate verification
        Profile3_TLS_Client = 3     // TLS with client certificate authentication (mTLS)
    };
    
    /**
     * @brief Certificate types for OCPP security
     */
    enum class CertificateType {
        CentralSystemRootCert,      // Root CA certificate for Central System
        ManufacturerRootCert,       // Root CA certificate for Manufacturer
        ChargePointCert,            // Client certificate for Charge Point
        V2GRootCert,                // Root CA certificate for V2G
        V2GCert                     // Client certificate for V2G
    };
    
    /**
     * @brief Certificate status as defined in OCPP
     */
    enum class CertificateStatus {
        Accepted,
        Rejected,
        Failed
    };
    
    /**
     * @brief Security configuration for WebSocket connections
     */
    struct SecurityConfig {
        SecurityProfile profile = SecurityProfile::Profile1_NoSecurity;
        
        // Server certificate verification (Profile 2 & 3)
        std::string caCertificate;
        bool verifyServerCertificate = true;
        
        // Client certificate authentication (Profile 3 only)
        std::string clientCertificate;
        std::string clientPrivateKey;
        std::string clientPrivateKeyPassword;
        
        // Additional security settings
        std::vector<std::string> allowedCipherSuites;
        std::string sniServerName;
        bool verifyHostname = true;
        uint32_t handshakeTimeoutMs = 10000;
    };
    
    /**
     * @brief Certificate management interface for OCPP security
     */
    class ICertificateManager {
    public:
        virtual ~ICertificateManager() = default;
        
        // Certificate installation and management
        virtual CertificateStatus installCertificate(
            CertificateType type, 
            const std::string& certificate
        ) = 0;
        
        virtual bool deleteCertificate(CertificateType type) = 0;
        virtual std::string getCertificate(CertificateType type) = 0;
        virtual std::vector<std::string> getInstalledCertificates() = 0;
        
        // Certificate validation
        virtual bool validateCertificate(
            const std::string& certificate,
            CertificateType type = CertificateType::ChargePointCert
        ) = 0;
        
        virtual bool isCertificateExpired(const std::string& certificate) = 0;
        virtual std::string getCertificateSerialNumber(const std::string& certificate) = 0;
        
        // Key pair generation
        virtual std::pair<std::string, std::string> generateKeyPair(int keySize = 2048) = 0;
        virtual std::string generateCSR(
            const std::string& privateKey,
            const std::string& commonName,
            const std::string& organization = "",
            const std::string& country = ""
        ) = 0;
    };
    
    /**
     * @brief Enhanced WebSocket client interface with security profiles
     */
    class ISecureWebSocketClient {
    public:
        virtual ~ISecureWebSocketClient() = default;
        
        // Connection management with security
        virtual bool connect(const std::string& url, const SecurityConfig& securityConfig) = 0;
        virtual void disconnect() = 0;
        virtual bool isConnected() = 0;
        
        // Message handling
        virtual bool sendMessage(const std::string& message) = 0;
        virtual void setMessageCallback(std::function<void(const std::string&)> callback) = 0;
        virtual void setConnectionCallback(std::function<void(bool)> callback) = 0;
        virtual void setErrorCallback(std::function<void(const std::string&)> callback) = 0;
        
        // Security status
        virtual SecurityProfile getActiveSecurityProfile() = 0;
        virtual std::string getConnectionInfo() = 0;
        virtual bool isSecureConnection() = 0;
        
        // Heartbeat/ping management
        virtual void enableHeartbeat(uint32_t intervalSeconds) = 0;
        virtual void disableHeartbeat() = 0;
    };
}