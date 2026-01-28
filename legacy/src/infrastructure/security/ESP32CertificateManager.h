#pragma once

#include <LittleFS.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/x509_csr.h>
#include <mbedtls/rsa.h>
#include <mbedtls/error.h>

#include "../ocpp/SecurityProfiles.h"

namespace Infrastructure {
    
    /**
     * @brief ESP32 implementation of certificate manager using LittleFS and mbedTLS
     * 
     * Follows SOLID principles:
     * - Single Responsibility: Only handles certificate management operations
     * - Open/Closed: Extensible for new certificate types without modification
     * - Liskov Substitution: Fully implements ICertificateManager interface
     * - Interface Segregation: Implements only certificate-related operations
     * - Dependency Inversion: Depends on abstractions (ICertificateManager)
     */
    class ESP32CertificateManager : public ICertificateManager {
    private:
        // Certificate file paths in LittleFS (following clean file organization)
        static constexpr const char* CERT_BASE_PATH = "/certs";
        static constexpr const char* CA_CERT_PATH = "/certs/ca.pem";
        static constexpr const char* CLIENT_CERT_PATH = "/certs/client.pem";
        static constexpr const char* CLIENT_KEY_PATH = "/certs/client.key";
        static constexpr const char* MANUFACTURER_CA_PATH = "/certs/mfg_ca.pem";
        static constexpr const char* V2G_CA_PATH = "/certs/v2g_ca.pem";
        static constexpr const char* V2G_CERT_PATH = "/certs/v2g.pem";
        static constexpr const char* V2G_KEY_PATH = "/certs/v2g.key";
        
        // Certificate backup and metadata paths
        static constexpr const char* CERT_METADATA_PATH = "/certs/metadata.json";
        static constexpr const char* CERT_BACKUP_PATH = "/backup/certs";
        
        // mbedTLS contexts for cryptographic operations
        mbedtls_ctr_drbg_context ctr_drbg;
        mbedtls_entropy_context entropy;
        
        // Internal state
        bool initialized = false;
        bool fileSystemReady = false;
        
        // Certificate validation configuration
        struct ValidationConfig {
            bool checkExpiry = true;
            bool checkCertificateChain = true;
            bool allowSelfSigned = false;
            uint32_t maxCertificateSize = 8192;
            uint32_t maxKeySize = 4096;
        } validationConfig;
        
        // Helper methods following Single Responsibility Principle
        std::string getCertificatePath(CertificateType type) const;
        std::string getPrivateKeyPath(CertificateType type) const;
        std::string getCertificateTypeString(CertificateType type) const;
        
        // File system operations
        bool ensureDirectoryExists(const std::string& path);
        bool validateFileSystemSpace(size_t requiredBytes);
        bool writeSecureFile(const std::string& path, const std::string& content);
        std::string readSecureFile(const std::string& path);
        
        // Certificate validation helpers
        bool validateCertificateFormat(const std::string& certificate);
        bool validatePrivateKeyFormat(const std::string& privateKey);
        bool validateCertificateChain(const std::string& certificate, const std::string& caCertificate);
        bool checkCertificateExpiry(const mbedtls_x509_crt* cert);
        
        // Certificate parsing and information extraction
        std::string extractCertificateInfo(const std::string& certificate, const std::string& field);
        time_t getCertificateExpiry(const mbedtls_x509_crt* cert);
        std::string getCertificateSubject(const mbedtls_x509_crt* cert);
        std::string getCertificateIssuer(const mbedtls_x509_crt* cert);
        
        // Cryptographic operations
        bool initializeMbedTLS();
        void cleanupMbedTLS();
        std::string formatMbedTLSError(int error);
        
        // Certificate metadata management
        bool saveCertificateMetadata(CertificateType type, const std::string& certificate);
        bool loadCertificateMetadata(CertificateType type);
        void updateCertificateMetadata();
        
        // Security validation
        bool verifyKeyPairMatch(const std::string& certificate, const std::string& privateKey);
        bool checkCertificateUsage(const mbedtls_x509_crt* cert, CertificateType type);
        
    public:
        ESP32CertificateManager();
        ~ESP32CertificateManager() override;
        
        // Lifecycle management
        bool initialize();
        void cleanup();
        bool isInitialized() const { return initialized; }
        
        // ICertificateManager interface implementation
        CertificateStatus installCertificate(CertificateType type, const std::string& certificate) override;
        bool deleteCertificate(CertificateType type) override;
        std::string getCertificate(CertificateType type) override;
        std::vector<std::string> getInstalledCertificates() override;
        
        bool validateCertificate(const std::string& certificate, CertificateType type) override;
        bool isCertificateExpired(const std::string& certificate) override;
        std::string getCertificateSerialNumber(const std::string& certificate) override;
        
        std::pair<std::string, std::string> generateKeyPair(int keySize = 2048) override;
        std::string generateCSR(
            const std::string& privateKey,
            const std::string& commonName,
            const std::string& organization = "",
            const std::string& country = ""
        ) override;
        
        // Extended certificate operations
        bool installKeyPair(CertificateType type, const std::string& certificate, const std::string& privateKey);
        bool exportCertificate(CertificateType type, const std::string& exportPath);
        bool importCertificate(CertificateType type, const std::string& importPath);
        
        // Certificate information and diagnostics
        struct CertificateInfo {
            std::string subject;
            std::string issuer;
            std::string serialNumber;
            std::string validFrom;
            std::string validTo;
            bool isExpired;
            int keySize;
            std::string signatureAlgorithm;
        };
        
        CertificateInfo getCertificateInfo(CertificateType type);
        CertificateInfo analyzeCertificate(const std::string& certificate);
        
        // Storage management
        size_t getAvailableStorage() const;
        size_t getCertificateStorageUsage() const;
        bool cleanupExpiredCertificates();
        
        // Backup and restore operations
        bool backupCertificates(const std::string& backupPath);
        bool restoreCertificates(const std::string& backupPath);
        bool createCertificateBundle(const std::string& bundlePath);
        
        // Security configuration helpers for OCPP profiles
        SecurityConfig createSecurityConfigProfile1();
        SecurityConfig createSecurityConfigProfile2(const std::string& serverName = "");
        SecurityConfig createSecurityConfigProfile3(const std::string& serverName = "");
        
        // Certificate validation configuration
        void setValidationConfig(const ValidationConfig& config) { validationConfig = config; }
        ValidationConfig getValidationConfig() const { return validationConfig; }
        
        // Certificate renewal and lifecycle management
        bool isCertificateNearExpiry(CertificateType type, uint32_t daysThreshold = 30);
        std::vector<CertificateType> getCertificatesNearExpiry(uint32_t daysThreshold = 30);
        bool renewCertificate(CertificateType type);
        
        // Factory method for creating configured instance
        static std::unique_ptr<ESP32CertificateManager> create();
        
        // Error handling and diagnostics
        enum class CertificateManagerError {
            NONE,
            FILESYSTEM_ERROR,
            CERTIFICATE_PARSE_ERROR,
            KEY_PARSE_ERROR,
            VALIDATION_FAILED,
            STORAGE_FULL,
            CERTIFICATE_EXPIRED,
            KEY_MISMATCH,
            UNSUPPORTED_FORMAT
        };
        
        CertificateManagerError getLastError() const { return lastError; }
        std::string getLastErrorString() const;
        void printDiagnostics() const;
        
    private:
        mutable CertificateManagerError lastError = CertificateManagerError::NONE;
        void setLastError(CertificateManagerError error) const { lastError = error; }
    };
}