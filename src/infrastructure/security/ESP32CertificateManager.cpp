#include "ESP32CertificateManager.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <mbedtls/x509.h>
#include <mbedtls/pk.h>
#include <mbedtls/oid.h>

using namespace Infrastructure;

ESP32CertificateManager::ESP32CertificateManager() {
    // Initialize mbedTLS contexts
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
}

ESP32CertificateManager::~ESP32CertificateManager() {
    cleanup();
}

bool ESP32CertificateManager::initialize() {
    if (initialized) {
        return true;
    }
    
    Serial.println("Initializing ESP32 Certificate Manager...");
    
    // Initialize LittleFS if not already done
    if (!LittleFS.begin()) {
        Serial.println("Failed to initialize LittleFS");
        setLastError(CertificateManagerError::FILESYSTEM_ERROR);
        return false;
    }
    fileSystemReady = true;
    
    // Initialize mbedTLS
    if (!initializeMbedTLS()) {
        Serial.println("Failed to initialize mbedTLS");
        return false;
    }
    
    // Create certificate directory if it doesn't exist
    if (!ensureDirectoryExists(CERT_BASE_PATH)) {
        Serial.println("Failed to create certificate directory");
        setLastError(CertificateManagerError::FILESYSTEM_ERROR);
        return false;
    }
    
    // Load existing certificate metadata
    updateCertificateMetadata();
    
    initialized = true;
    Serial.println("Certificate Manager initialized successfully");
    
    return true;
}

void ESP32CertificateManager::cleanup() {
    cleanupMbedTLS();
    
    if (fileSystemReady) {
        // LittleFS.end(); // Uncomment if you want to fully unmount
    }
    
    initialized = false;
    fileSystemReady = false;
}

bool ESP32CertificateManager::initializeMbedTLS() {
    const char* personalization = "esp32_ocpp_cert_manager";
    
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                   (const unsigned char*)personalization,
                                   strlen(personalization));
    if (ret != 0) {
        Serial.printf("mbedTLS initialization failed: %s\n", formatMbedTLSError(ret).c_str());
        return false;
    }
    
    return true;
}

void ESP32CertificateManager::cleanupMbedTLS() {
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}

std::string ESP32CertificateManager::formatMbedTLSError(int error) {
    char error_buf[256];
    mbedtls_strerror(error, error_buf, sizeof(error_buf));
    return std::string(error_buf);
}

std::string ESP32CertificateManager::getCertificatePath(CertificateType type) const {
    switch (type) {
        case CertificateType::CentralSystemRootCert:
            return CA_CERT_PATH;
        case CertificateType::ChargePointCert:
            return CLIENT_CERT_PATH;
        case CertificateType::ManufacturerRootCert:
            return MANUFACTURER_CA_PATH;
        case CertificateType::V2GRootCert:
            return V2G_CA_PATH;
        case CertificateType::V2GCert:
            return V2G_CERT_PATH;
        default:
            return "";
    }
}

std::string ESP32CertificateManager::getPrivateKeyPath(CertificateType type) const {
    switch (type) {
        case CertificateType::ChargePointCert:
            return CLIENT_KEY_PATH;
        case CertificateType::V2GCert:
            return V2G_KEY_PATH;
        default:
            return ""; // CA certificates don't have private keys stored
    }
}

std::string ESP32CertificateManager::getCertificateTypeString(CertificateType type) const {
    switch (type) {
        case CertificateType::CentralSystemRootCert:
            return "Central System Root CA";
        case CertificateType::ChargePointCert:
            return "Charge Point Certificate";
        case CertificateType::ManufacturerRootCert:
            return "Manufacturer Root CA";
        case CertificateType::V2GRootCert:
            return "V2G Root CA";
        case CertificateType::V2GCert:
            return "V2G Certificate";
        default:
            return "Unknown";
    }
}

bool ESP32CertificateManager::ensureDirectoryExists(const std::string& path) {
    if (!fileSystemReady) {
        return false;
    }
    
    // LittleFS doesn't have explicit directory creation, but we can check if we can write to the path
    File testFile = LittleFS.open((path + "/.test").c_str(), "w");
    if (!testFile) {
        return false;
    }
    testFile.close();
    LittleFS.remove((path + "/.test").c_str());
    
    return true;
}

bool ESP32CertificateManager::validateFileSystemSpace(size_t requiredBytes) {
    if (!fileSystemReady) {
        return false;
    }
    
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    size_t availableBytes = totalBytes - usedBytes;
    
    return availableBytes >= requiredBytes;
}

bool ESP32CertificateManager::writeSecureFile(const std::string& path, const std::string& content) {
    if (!fileSystemReady) {
        setLastError(CertificateManagerError::FILESYSTEM_ERROR);
        return false;
    }
    
    // Check available space
    if (!validateFileSystemSpace(content.length() + 1024)) { // Add some buffer
        setLastError(CertificateManagerError::STORAGE_FULL);
        return false;
    }
    
    File file = LittleFS.open(path.c_str(), "w");
    if (!file) {
        Serial.printf("Failed to open file for writing: %s\n", path.c_str());
        setLastError(CertificateManagerError::FILESYSTEM_ERROR);
        return false;
    }
    
    size_t bytesWritten = file.write((const uint8_t*)content.c_str(), content.length());
    file.close();
    
    if (bytesWritten != content.length()) {
        Serial.printf("Failed to write complete content to file: %s\n", path.c_str());
        setLastError(CertificateManagerError::FILESYSTEM_ERROR);
        return false;
    }
    
    Serial.printf("Successfully wrote %d bytes to %s\n", bytesWritten, path.c_str());
    return true;
}

std::string ESP32CertificateManager::readSecureFile(const std::string& path) {
    if (!fileSystemReady) {
        setLastError(CertificateManagerError::FILESYSTEM_ERROR);
        return "";
    }
    
    File file = LittleFS.open(path.c_str(), "r");
    if (!file) {
        Serial.printf("Failed to open file for reading: %s\n", path.c_str());
        setLastError(CertificateManagerError::FILESYSTEM_ERROR);
        return "";
    }
    
    String content = file.readString();
    file.close();
    
    return content.c_str();
}

CertificateStatus ESP32CertificateManager::installCertificate(CertificateType type, const std::string& certificate) {
    if (!initialized) {
        Serial.println("Certificate manager not initialized");
        setLastError(CertificateManagerError::FILESYSTEM_ERROR);
        return CertificateStatus::Failed;
    }
    
    Serial.printf("Installing certificate: %s\n", getCertificateTypeString(type).c_str());
    
    // Validate certificate format
    if (!validateCertificate(certificate, type)) {
        Serial.println("Certificate validation failed");
        return CertificateStatus::Rejected;
    }
    
    // Get certificate path
    std::string certPath = getCertificatePath(type);
    if (certPath.empty()) {
        Serial.println("Invalid certificate type");
        setLastError(CertificateManagerError::UNSUPPORTED_FORMAT);
        return CertificateStatus::Failed;
    }
    
    // Write certificate to file system
    if (!writeSecureFile(certPath, certificate)) {
        Serial.println("Failed to write certificate to storage");
        return CertificateStatus::Failed;
    }
    
    // Save certificate metadata
    if (!saveCertificateMetadata(type, certificate)) {
        Serial.println("Warning: Failed to save certificate metadata");
    }
    
    Serial.printf("Certificate installed successfully: %s\n", getCertificateTypeString(type).c_str());
    return CertificateStatus::Accepted;
}

bool ESP32CertificateManager::deleteCertificate(CertificateType type) {
    if (!initialized) {
        return false;
    }
    
    std::string certPath = getCertificatePath(type);
    std::string keyPath = getPrivateKeyPath(type);
    
    bool success = true;
    
    // Delete certificate file
    if (!certPath.empty() && LittleFS.exists(certPath.c_str())) {
        if (!LittleFS.remove(certPath.c_str())) {
            Serial.printf("Failed to delete certificate: %s\n", certPath.c_str());
            success = false;
        } else {
            Serial.printf("Certificate deleted: %s\n", certPath.c_str());
        }\n    }\n    \n    // Delete private key file if it exists\n    if (!keyPath.empty() && LittleFS.exists(keyPath.c_str())) {\n        if (!LittleFS.remove(keyPath.c_str())) {\n            Serial.printf(\"Failed to delete private key: %s\\n\", keyPath.c_str());\n            success = false;\n        } else {\n            Serial.printf(\"Private key deleted: %s\\n\", keyPath.c_str());\n        }\n    }\n    \n    if (success) {\n        Serial.printf(\"Certificate deleted successfully: %s\\n\", getCertificateTypeString(type).c_str());\n    }\n    \n    return success;\n}\n\nstd::string ESP32CertificateManager::getCertificate(CertificateType type) {\n    if (!initialized) {\n        return \"\";\n    }\n    \n    std::string certPath = getCertificatePath(type);\n    if (certPath.empty()) {\n        return \"\";\n    }\n    \n    return readSecureFile(certPath);\n}\n\nstd::vector<std::string> ESP32CertificateManager::getInstalledCertificates() {\n    std::vector<std::string> certificates;\n    \n    if (!initialized) {\n        return certificates;\n    }\n    \n    // Check each certificate type\n    std::vector<CertificateType> types = {\n        CertificateType::CentralSystemRootCert,\n        CertificateType::ChargePointCert,\n        CertificateType::ManufacturerRootCert,\n        CertificateType::V2GRootCert,\n        CertificateType::V2GCert\n    };\n    \n    for (CertificateType type : types) {\n        std::string certPath = getCertificatePath(type);\n        if (!certPath.empty() && LittleFS.exists(certPath.c_str())) {\n            certificates.push_back(getCertificateTypeString(type));\n        }\n    }\n    \n    return certificates;\n}\n\nbool ESP32CertificateManager::validateCertificate(const std::string& certificate, CertificateType type) {\n    if (certificate.empty()) {\n        setLastError(CertificateManagerError::CERTIFICATE_PARSE_ERROR);\n        return false;\n    }\n    \n    // Basic format validation\n    if (!validateCertificateFormat(certificate)) {\n        return false;\n    }\n    \n    mbedtls_x509_crt cert;\n    mbedtls_x509_crt_init(&cert);\n    \n    int ret = mbedtls_x509_crt_parse(&cert, \n                                   (const unsigned char*)certificate.c_str(), \n                                   certificate.length() + 1);\n    \n    if (ret != 0) {\n        Serial.printf(\"Certificate parse error: %s\\n\", formatMbedTLSError(ret).c_str());\n        mbedtls_x509_crt_free(&cert);\n        setLastError(CertificateManagerError::CERTIFICATE_PARSE_ERROR);\n        return false;\n    }\n    \n    // Check expiry if configured\n    bool valid = true;\n    if (validationConfig.checkExpiry && checkCertificateExpiry(&cert)) {\n        Serial.println(\"Certificate has expired\");\n        setLastError(CertificateManagerError::CERTIFICATE_EXPIRED);\n        valid = false;\n    }\n    \n    // Check certificate usage based on type\n    if (valid && !checkCertificateUsage(&cert, type)) {\n        Serial.println(\"Certificate usage validation failed\");\n        setLastError(CertificateManagerError::VALIDATION_FAILED);\n        valid = false;\n    }\n    \n    mbedtls_x509_crt_free(&cert);\n    return valid;\n}\n\nbool ESP32CertificateManager::validateCertificateFormat(const std::string& certificate) {\n    // Check basic PEM format\n    if (certificate.find(\"-----BEGIN CERTIFICATE-----\") == std::string::npos ||\n        certificate.find(\"-----END CERTIFICATE-----\") == std::string::npos) {\n        Serial.println(\"Invalid certificate format - not PEM\");\n        setLastError(CertificateManagerError::UNSUPPORTED_FORMAT);\n        return false;\n    }\n    \n    // Check size limits\n    if (certificate.length() > validationConfig.maxCertificateSize) {\n        Serial.printf(\"Certificate too large: %d bytes (max %d)\\n\", \n                     certificate.length(), validationConfig.maxCertificateSize);\n        setLastError(CertificateManagerError::UNSUPPORTED_FORMAT);\n        return false;\n    }\n    \n    return true;\n}\n\nbool ESP32CertificateManager::checkCertificateExpiry(const mbedtls_x509_crt* cert) {\n    if (!cert) {\n        return true; // Assume expired if we can't check\n    }\n    \n    time_t now;\n    time(&now);\n    \n    // Compare with certificate's valid_to time\n    // Note: This is a simplified check. Full implementation would need proper time comparison\n    // mbedTLS stores time as mbedtls_x509_time structure\n    \n    return false; // For now, assume not expired\n}\n\nbool ESP32CertificateManager::checkCertificateUsage(const mbedtls_x509_crt* cert, CertificateType type) {\n    if (!cert) {\n        return false;\n    }\n    \n    // Check key usage and extended key usage based on certificate type\n    // This would involve parsing the certificate extensions\n    // For now, we'll do basic validation\n    \n    switch (type) {\n        case CertificateType::CentralSystemRootCert:\n        case CertificateType::ManufacturerRootCert:\n        case CertificateType::V2GRootCert:\n            // CA certificates should have certificate signing capability\n            return (cert->ca_istrue != 0);\n            \n        case CertificateType::ChargePointCert:\n        case CertificateType::V2GCert:\n            // Client certificates should not be CA certificates\n            return (cert->ca_istrue == 0);\n            \n        default:\n            return true;\n    }\n}\n\nbool ESP32CertificateManager::isCertificateExpired(const std::string& certificate) {\n    if (certificate.empty()) {\n        return true;\n    }\n    \n    mbedtls_x509_crt cert;\n    mbedtls_x509_crt_init(&cert);\n    \n    int ret = mbedtls_x509_crt_parse(&cert, \n                                   (const unsigned char*)certificate.c_str(), \n                                   certificate.length() + 1);\n    \n    if (ret != 0) {\n        mbedtls_x509_crt_free(&cert);\n        return true; // Assume expired if we can't parse\n    }\n    \n    bool expired = checkCertificateExpiry(&cert);\n    mbedtls_x509_crt_free(&cert);\n    \n    return expired;\n}\n\nstd::string ESP32CertificateManager::getCertificateSerialNumber(const std::string& certificate) {\n    if (certificate.empty()) {\n        return \"\";\n    }\n    \n    mbedtls_x509_crt cert;\n    mbedtls_x509_crt_init(&cert);\n    \n    int ret = mbedtls_x509_crt_parse(&cert, \n                                   (const unsigned char*)certificate.c_str(), \n                                   certificate.length() + 1);\n    \n    if (ret != 0) {\n        mbedtls_x509_crt_free(&cert);\n        return \"\";\n    }\n    \n    // Convert serial number to hex string\n    std::string serialHex = \"\";\n    for (size_t i = 0; i < cert.serial.len; i++) {\n        char hex[3];\n        sprintf(hex, \"%02x\", cert.serial.p[i]);\n        serialHex += hex;\n    }\n    \n    mbedtls_x509_crt_free(&cert);\n    return serialHex;\n}\n\nstd::pair<std::string, std::string> ESP32CertificateManager::generateKeyPair(int keySize) {\n    Serial.printf(\"Generating %d-bit RSA key pair...\\n\", keySize);\n    \n    mbedtls_pk_context key;\n    mbedtls_pk_init(&key);\n    \n    // Generate RSA key pair\n    int ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));\n    if (ret != 0) {\n        Serial.printf(\"Key setup failed: %s\\n\", formatMbedTLSError(ret).c_str());\n        mbedtls_pk_free(&key);\n        return std::make_pair(\"\", \"\");\n    }\n    \n    ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(key), mbedtls_ctr_drbg_random, &ctr_drbg, keySize, 65537);\n    if (ret != 0) {\n        Serial.printf(\"Key generation failed: %s\\n\", formatMbedTLSError(ret).c_str());\n        mbedtls_pk_free(&key);\n        return std::make_pair(\"\", \"\");\n    }\n    \n    // Export public key\n    unsigned char pubkey_buf[4096];\n    int pubkey_len = mbedtls_pk_write_pubkey_pem(&key, pubkey_buf, sizeof(pubkey_buf));\n    std::string publicKey = \"\";\n    if (pubkey_len > 0) {\n        publicKey = std::string((char*)pubkey_buf);\n    }\n    \n    // Export private key\n    unsigned char privkey_buf[4096];\n    int privkey_len = mbedtls_pk_write_key_pem(&key, privkey_buf, sizeof(privkey_buf));\n    std::string privateKey = \"\";\n    if (privkey_len > 0) {\n        privateKey = std::string((char*)privkey_buf);\n    }\n    \n    mbedtls_pk_free(&key);\n    \n    if (publicKey.empty() || privateKey.empty()) {\n        Serial.println(\"Failed to export generated key pair\");\n        return std::make_pair(\"\", \"\");\n    }\n    \n    Serial.println(\"Key pair generated successfully\");\n    return std::make_pair(publicKey, privateKey);\n}\n\nstd::string ESP32CertificateManager::generateCSR(\n    const std::string& privateKey,\n    const std::string& commonName,\n    const std::string& organization,\n    const std::string& country) {\n    \n    Serial.println(\"Generating Certificate Signing Request...\");\n    \n    mbedtls_x509write_csr csr;\n    mbedtls_pk_context key;\n    \n    mbedtls_x509write_csr_init(&csr);\n    mbedtls_pk_init(&key);\n    \n    // Parse private key\n    int ret = mbedtls_pk_parse_key(&key, \n                                 (const unsigned char*)privateKey.c_str(), \n                                 privateKey.length() + 1, \n                                 nullptr, 0,\n                                 mbedtls_ctr_drbg_random, &ctr_drbg);\n    \n    if (ret != 0) {\n        Serial.printf(\"Private key parse failed: %s\\n\", formatMbedTLSError(ret).c_str());\n        mbedtls_pk_free(&key);\n        mbedtls_x509write_csr_free(&csr);\n        return \"\";\n    }\n    \n    // Set key for CSR\n    mbedtls_x509write_csr_set_key(&csr, &key);\n    \n    // Build subject name\n    std::string subject = \"CN=\" + commonName;\n    if (!organization.empty()) {\n        subject += \",O=\" + organization;\n    }\n    if (!country.empty()) {\n        subject += \",C=\" + country;\n    }\n    \n    // Set subject\n    ret = mbedtls_x509write_csr_set_subject_name(&csr, subject.c_str());\n    if (ret != 0) {\n        Serial.printf(\"Subject name set failed: %s\\n\", formatMbedTLSError(ret).c_str());\n        mbedtls_pk_free(&key);\n        mbedtls_x509write_csr_free(&csr);\n        return \"\";\n    }\n    \n    // Set hash algorithm\n    mbedtls_x509write_csr_set_md_alg(&csr, MBEDTLS_MD_SHA256);\n    \n    // Generate CSR\n    unsigned char csr_buf[4096];\n    ret = mbedtls_x509write_csr_pem(&csr, csr_buf, sizeof(csr_buf), \n                                  mbedtls_ctr_drbg_random, &ctr_drbg);\n    \n    mbedtls_pk_free(&key);\n    mbedtls_x509write_csr_free(&csr);\n    \n    if (ret < 0) {\n        Serial.printf(\"CSR generation failed: %s\\n\", formatMbedTLSError(ret).c_str());\n        return \"\";\n    }\n    \n    std::string csrPem = std::string((char*)csr_buf);\n    Serial.println(\"CSR generated successfully\");\n    \n    return csrPem;\n}\n\nsize_t ESP32CertificateManager::getAvailableStorage() const {\n    if (!fileSystemReady) {\n        return 0;\n    }\n    \n    return LittleFS.totalBytes() - LittleFS.usedBytes();\n}\n\nsize_t ESP32CertificateManager::getCertificateStorageUsage() const {\n    if (!fileSystemReady) {\n        return 0;\n    }\n    \n    size_t totalUsage = 0;\n    \n    // Check certificate directory\n    File dir = LittleFS.open(CERT_BASE_PATH);\n    if (dir && dir.isDirectory()) {\n        File file = dir.openNextFile();\n        while (file) {\n            if (!file.isDirectory()) {\n                totalUsage += file.size();\n            }\n            file = dir.openNextFile();\n        }\n    }\n    \n    return totalUsage;\n}\n\nbool ESP32CertificateManager::saveCertificateMetadata(CertificateType type, const std::string& certificate) {\n    // This would save metadata like installation date, expiry, etc.\n    // For now, we'll implement a basic version\n    \n    JsonDocument metadata;\n    metadata[\"type\"] = getCertificateTypeString(type);\n    metadata[\"installed\"] = millis();\n    metadata[\"serialNumber\"] = getCertificateSerialNumber(certificate);\n    \n    std::string metadataStr;\n    serializeJson(metadata, metadataStr);\n    \n    std::string metadataPath = std::string(CERT_BASE_PATH) + \"/\" + \n                              getCertificateTypeString(type) + \"_metadata.json\";\n    \n    return writeSecureFile(metadataPath, metadataStr);\n}\n\nbool ESP32CertificateManager::loadCertificateMetadata(CertificateType type) {\n    // Load and validate metadata for a certificate\n    std::string metadataPath = std::string(CERT_BASE_PATH) + \"/\" + \n                              getCertificateTypeString(type) + \"_metadata.json\";\n    \n    std::string metadataStr = readSecureFile(metadataPath);\n    if (metadataStr.empty()) {\n        return false;\n    }\n    \n    JsonDocument metadata;\n    DeserializationError error = deserializeJson(metadata, metadataStr);\n    if (error) {\n        Serial.printf(\"Failed to parse certificate metadata: %s\\n\", error.c_str());\n        return false;\n    }\n    \n    return true;\n}\n\nvoid ESP32CertificateManager::updateCertificateMetadata() {\n    // Update metadata for all installed certificates\n    std::vector<CertificateType> types = {\n        CertificateType::CentralSystemRootCert,\n        CertificateType::ChargePointCert,\n        CertificateType::ManufacturerRootCert,\n        CertificateType::V2GRootCert,\n        CertificateType::V2GCert\n    };\n    \n    for (CertificateType type : types) {\n        std::string certificate = getCertificate(type);\n        if (!certificate.empty()) {\n            loadCertificateMetadata(type);\n        }\n    }\n}\n\nSecurityConfig ESP32CertificateManager::createSecurityConfigProfile1() {\n    SecurityConfig config;\n    config.profile = SecurityProfile::Profile1_NoSecurity;\n    return config;\n}\n\nSecurityConfig ESP32CertificateManager::createSecurityConfigProfile2(const std::string& serverName) {\n    SecurityConfig config;\n    config.profile = SecurityProfile::Profile2_TLS;\n    config.verifyServerCertificate = true;\n    config.verifyHostname = true;\n    \n    // Load CA certificate\n    std::string caCert = getCertificate(CertificateType::CentralSystemRootCert);\n    if (!caCert.empty()) {\n        config.caCertificate = caCert;\n    }\n    \n    if (!serverName.empty()) {\n        config.sniServerName = serverName;\n    }\n    \n    return config;\n}\n\nSecurityConfig ESP32CertificateManager::createSecurityConfigProfile3(const std::string& serverName) {\n    SecurityConfig config = createSecurityConfigProfile2(serverName);\n    config.profile = SecurityProfile::Profile3_TLS_Client;\n    \n    // Load client certificate and private key\n    std::string clientCert = getCertificate(CertificateType::ChargePointCert);\n    if (!clientCert.empty()) {\n        config.clientCertificate = clientCert;\n    }\n    \n    std::string privateKey = readSecureFile(getPrivateKeyPath(CertificateType::ChargePointCert));\n    if (!privateKey.empty()) {\n        config.clientPrivateKey = privateKey;\n    }\n    \n    return config;\n}\n\nstd::string ESP32CertificateManager::getLastErrorString() const {\n    switch (lastError) {\n        case CertificateManagerError::NONE:\n            return \"No error\";\n        case CertificateManagerError::FILESYSTEM_ERROR:\n            return \"Filesystem error\";\n        case CertificateManagerError::CERTIFICATE_PARSE_ERROR:\n            return \"Certificate parse error\";\n        case CertificateManagerError::KEY_PARSE_ERROR:\n            return \"Private key parse error\";\n        case CertificateManagerError::VALIDATION_FAILED:\n            return \"Certificate validation failed\";\n        case CertificateManagerError::STORAGE_FULL:\n            return \"Storage full\";\n        case CertificateManagerError::CERTIFICATE_EXPIRED:\n            return \"Certificate expired\";\n        case CertificateManagerError::KEY_MISMATCH:\n            return \"Key pair mismatch\";\n        case CertificateManagerError::UNSUPPORTED_FORMAT:\n            return \"Unsupported format\";\n        default:\n            return \"Unknown error\";\n    }\n}\n\nvoid ESP32CertificateManager::printDiagnostics() const {\n    Serial.println(\"\\n=== Certificate Manager Diagnostics ===\");\n    Serial.printf(\"Initialized: %s\\n\", initialized ? \"Yes\" : \"No\");\n    Serial.printf(\"Filesystem Ready: %s\\n\", fileSystemReady ? \"Yes\" : \"No\");\n    Serial.printf(\"Total Storage: %d bytes\\n\", LittleFS.totalBytes());\n    Serial.printf(\"Used Storage: %d bytes\\n\", LittleFS.usedBytes());\n    Serial.printf(\"Available Storage: %d bytes\\n\", getAvailableStorage());\n    Serial.printf(\"Certificate Storage Usage: %d bytes\\n\", getCertificateStorageUsage());\n    Serial.printf(\"Last Error: %s\\n\", getLastErrorString().c_str());\n    \n    Serial.println(\"\\nInstalled Certificates:\");\n    auto certs = getInstalledCertificates();\n    for (const auto& cert : certs) {\n        Serial.printf(\"  - %s\\n\", cert.c_str());\n    }\n    \n    Serial.println(\"==================================\\n\");\n}\n\nstd::unique_ptr<ESP32CertificateManager> ESP32CertificateManager::create() {\n    auto manager = std::make_unique<ESP32CertificateManager>();\n    \n    if (!manager->initialize()) {\n        Serial.println(\"Failed to create certificate manager\");\n        return nullptr;\n    }\n    \n    return manager;\n}"