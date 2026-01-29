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
    
    Serial.printf("Certificate installed successfully: %s\\n", getCertificateTypeString(type).c_str());
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
        }
    }

    // Delete private key file if present
    if (!keyPath.empty() && LittleFS.exists(keyPath.c_str())) {
        if (!LittleFS.remove(keyPath.c_str())) {
            Serial.printf("Failed to delete private key: %s\n", keyPath.c_str());
            success = false;
        } else {
            Serial.printf("Private key deleted: %s\n", keyPath.c_str());
        }
    }

    if (success) {
        Serial.printf("Certificate deleted successfully: %s\n", getCertificateTypeString(type).c_str());
    }

    return success;
}

std::string ESP32CertificateManager::getCertificate(CertificateType type) {
    if (!initialized) {
        return \"\";
    }
    
    std::string certPath = getCertificatePath(type);
    if (certPath.empty()) {
        return \"\";
    }
    
    return readSecureFile(certPath);
}

std::vector<std::string> ESP32CertificateManager::getInstalledCertificates() {
    std::vector<std::string> certificates;

    if (!initialized) {
        return certificates;
    }

    // Check each certificate type
    std::vector<CertificateType> types = {
        CertificateType::CentralSystemRootCert,
        CertificateType::ChargePointCert,
        CertificateType::ManufacturerRootCert,
        CertificateType::V2GRootCert,
        CertificateType::V2GCert};

    for (CertificateType type : types) {
        std::string certPath = getCertificatePath(type);
        if (!certPath.empty() && LittleFS.exists(certPath.c_str())) {
            certificates.push_back(getCertificateTypeString(type));
        }
    }

    return certificates;
}

bool ESP32CertificateManager::validateCertificate(const std::string& certificate, CertificateType type) {
    if (certificate.empty()) {
        setLastError(CertificateManagerError::CERTIFICATE_PARSE_ERROR);
        return false;
    }

    // Basic format validation
    if (!validateCertificateFormat(certificate)) {
        return false;
    }

    mbedtls_x509_crt cert;
    mbedtls_x509_crt_init(&cert);

    int ret = mbedtls_x509_crt_parse(
        &cert,
        reinterpret_cast<const unsigned char*>(certificate.c_str()),
        certificate.length() + 1);

    if (ret != 0) {
        Serial.printf("Certificate parse error: %s\n", formatMbedTLSError(ret).c_str());
        mbedtls_x509_crt_free(&cert);
        setLastError(CertificateManagerError::CERTIFICATE_PARSE_ERROR);
        return false;
    }

    // Check expiry if configured
    bool valid = true;
    if (validationConfig.checkExpiry && checkCertificateExpiry(&cert)) {
        Serial.println("Certificate has expired");
        setLastError(CertificateManagerError::CERTIFICATE_EXPIRED);
        valid = false;
    }

    // Check certificate usage based on type
    if (valid && !checkCertificateUsage(&cert, type)) {
        Serial.println("Certificate usage validation failed");
        setLastError(CertificateManagerError::VALIDATION_FAILED);
        valid = false;
    }

    mbedtls_x509_crt_free(&cert);
    return valid;
}

bool ESP32CertificateManager::validateCertificateFormat(const std::string& certificate) {
    // Check basic PEM format
    if (certificate.find("-----BEGIN CERTIFICATE-----") == std::string::npos ||
        certificate.find("-----END CERTIFICATE-----") == std::string::npos) {
        Serial.println("Invalid certificate format - not PEM");
        setLastError(CertificateManagerError::UNSUPPORTED_FORMAT);
        return false;
    }

    // Check size limits
    if (certificate.length() > validationConfig.maxCertificateSize) {
        Serial.printf("Certificate too large: %d bytes (max %d)\n",
                      static_cast<int>(certificate.length()),
                      static_cast<int>(validationConfig.maxCertificateSize));
        setLastError(CertificateManagerError::UNSUPPORTED_FORMAT);
        return false;
    }

    return true;
}

bool ESP32CertificateManager::checkCertificateExpiry(const mbedtls_x509_crt* cert) {
    if (!cert) {
        // Assume expired if we can't check
        return true;
    }

    time_t now;
    time(&now);

    // TODO: Implement proper expiry check using cert->valid_to
    // For now, assume not expired
    return false;
}

bool ESP32CertificateManager::checkCertificateUsage(const mbedtls_x509_crt* cert, CertificateType type) {
    if (!cert) {
        return false;
    }

    // Basic validation using CA flag; more detailed checks could
    // inspect key usage and extended key usage extensions.
    switch (type) {
        case CertificateType::CentralSystemRootCert:
        case CertificateType::ManufacturerRootCert:
        case CertificateType::V2GRootCert:
            // CA certificates should have certificate signing capability
            return (cert->ca_istrue != 0);

        case CertificateType::ChargePointCert:
        case CertificateType::V2GCert:
            // Client certificates should not be CA certificates
            return (cert->ca_istrue == 0);

        default:
            return true;
    }
}

bool ESP32CertificateManager::isCertificateExpired(const std::string& certificate) {
    if (certificate.empty()) {
        return true;
    }

    mbedtls_x509_crt cert;
    mbedtls_x509_crt_init(&cert);

    int ret = mbedtls_x509_crt_parse(
        &cert,
        reinterpret_cast<const unsigned char*>(certificate.c_str()),
        certificate.length() + 1);

    if (ret != 0) {
        mbedtls_x509_crt_free(&cert);
        // Assume expired if we can't parse
        return true;
    }

    bool expired = checkCertificateExpiry(&cert);
    mbedtls_x509_crt_free(&cert);
    return expired;
}

std::string ESP32CertificateManager::getCertificateSerialNumber(const std::string& certificate) {
    if (certificate.empty()) {
        return std::string();
    }

    mbedtls_x509_crt cert;
    mbedtls_x509_crt_init(&cert);

    int ret = mbedtls_x509_crt_parse(
        &cert,
        reinterpret_cast<const unsigned char*>(certificate.c_str()),
        certificate.length() + 1);

    if (ret != 0) {
        mbedtls_x509_crt_free(&cert);
        return std::string();
    }

    // Convert serial number to hex string
    std::string serialHex;
    for (size_t i = 0; i < cert.serial.len; i++) {
        char hex[3];
        sprintf(hex, "%02x", cert.serial.p[i]);
        serialHex += hex;
    }

    mbedtls_x509_crt_free(&cert);
    return serialHex;
}

std::pair<std::string, std::string> ESP32CertificateManager::generateKeyPair(int keySize) {
    Serial.printf("Generating %d-bit RSA key pair...\n", keySize);

    mbedtls_pk_context key;
    mbedtls_pk_init(&key);

    // Generate RSA key pair
    int ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if (ret != 0) {
        Serial.printf("Key setup failed: %s\n", formatMbedTLSError(ret).c_str());
        mbedtls_pk_free(&key);
        return std::make_pair(std::string(), std::string());
    }

    ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(key), mbedtls_ctr_drbg_random, &ctr_drbg, keySize, 65537);
    if (ret != 0) {
        Serial.printf("Key generation failed: %s\n", formatMbedTLSError(ret).c_str());
        mbedtls_pk_free(&key);
        return std::make_pair(std::string(), std::string());
    }

    // Export public key
    unsigned char pubkey_buf[4096];
    int pubkey_len = mbedtls_pk_write_pubkey_pem(&key, pubkey_buf, sizeof(pubkey_buf));
    std::string publicKey;
    if (pubkey_len > 0) {
        publicKey = std::string(reinterpret_cast<char*>(pubkey_buf));
    }

    // Export private key
    unsigned char privkey_buf[4096];
    int privkey_len = mbedtls_pk_write_key_pem(&key, privkey_buf, sizeof(privkey_buf));
    std::string privateKey;
    if (privkey_len > 0) {
        privateKey = std::string(reinterpret_cast<char*>(privkey_buf));
    }

    mbedtls_pk_free(&key);

    if (publicKey.empty() || privateKey.empty()) {
        Serial.println("Failed to export generated key pair");
        return std::make_pair(std::string(), std::string());
    }

    Serial.println("Key pair generated successfully");
    return std::make_pair(publicKey, privateKey);
}

std::string ESP32CertificateManager::generateCSR(
    const std::string& privateKey,
    const std::string& commonName,
    const std::string& organization,
    const std::string& country) {

    Serial.println("Generating Certificate Signing Request...");

    mbedtls_x509write_csr csr;
    mbedtls_pk_context key;

    mbedtls_x509write_csr_init(&csr);
    mbedtls_pk_init(&key);

    // Parse private key
    int ret = mbedtls_pk_parse_key(
        &key,
        reinterpret_cast<const unsigned char*>(privateKey.c_str()),
        privateKey.length() + 1,
        nullptr,
        0,
        mbedtls_ctr_drbg_random,
        &ctr_drbg);

    if (ret != 0) {
        Serial.printf("Private key parse failed: %s\n", formatMbedTLSError(ret).c_str());
        mbedtls_pk_free(&key);
        mbedtls_x509write_csr_free(&csr);
        return std::string();
    }

    // Set key for CSR
    mbedtls_x509write_csr_set_key(&csr, &key);

    // Build subject name
    std::string subject = "CN=" + commonName;
    if (!organization.empty()) {
        subject += ",O=" + organization;
    }
    if (!country.empty()) {
        subject += ",C=" + country;
    }

    // Set subject
    ret = mbedtls_x509write_csr_set_subject_name(&csr, subject.c_str());
    if (ret != 0) {
        Serial.printf("Subject name set failed: %s\n", formatMbedTLSError(ret).c_str());
        mbedtls_pk_free(&key);
        mbedtls_x509write_csr_free(&csr);
        return std::string();
    }

    // Set hash algorithm
    mbedtls_x509write_csr_set_md_alg(&csr, MBEDTLS_MD_SHA256);

    // Generate CSR
    unsigned char csr_buf[4096];
    ret = mbedtls_x509write_csr_pem(&csr, csr_buf, sizeof(csr_buf),
                                    mbedtls_ctr_drbg_random, &ctr_drbg);

    mbedtls_pk_free(&key);
    mbedtls_x509write_csr_free(&csr);

    if (ret < 0) {
        Serial.printf("CSR generation failed: %s\n", formatMbedTLSError(ret).c_str());
        return std::string();
    }

    std::string csrPem = std::string(reinterpret_cast<char*>(csr_buf));
    Serial.println("CSR generated successfully");

    return csrPem;
}

size_t ESP32CertificateManager::getAvailableStorage() const {
    if (!fileSystemReady) {
        return 0;
    }

    return LittleFS.totalBytes() - LittleFS.usedBytes();
}

size_t ESP32CertificateManager::getCertificateStorageUsage() const {
    if (!fileSystemReady) {
        return 0;
    }

    size_t totalUsage = 0;

    // Check certificate directory
    File dir = LittleFS.open(CERT_BASE_PATH);
    if (dir && dir.isDirectory()) {
        File file = dir.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                totalUsage += file.size();
            }
            file = dir.openNextFile();
        }
    }

    return totalUsage;
}

bool ESP32CertificateManager::saveCertificateMetadata(CertificateType type, const std::string& certificate) {
    // This would save metadata like installation date, expiry, etc.
    // For now, we'll implement a basic version

    JsonDocument metadata;
    metadata["type"] = getCertificateTypeString(type);
    metadata["installed"] = millis();
    metadata["serialNumber"] = getCertificateSerialNumber(certificate);

    std::string metadataStr;
    serializeJson(metadata, metadataStr);

    std::string metadataPath = std::string(CERT_BASE_PATH) + "/" +
                               getCertificateTypeString(type) + "_metadata.json";

    return writeSecureFile(metadataPath, metadataStr);
}

bool ESP32CertificateManager::loadCertificateMetadata(CertificateType type) {
    // Load and validate metadata for a certificate
    std::string metadataPath = std::string(CERT_BASE_PATH) + "/" +
                               getCertificateTypeString(type) + "_metadata.json";

    std::string metadataStr = readSecureFile(metadataPath);
    if (metadataStr.empty()) {
        return false;
    }

    JsonDocument metadata;
    DeserializationError error = deserializeJson(metadata, metadataStr);
    if (error) {
        Serial.printf("Failed to parse certificate metadata: %s\n", error.c_str());
        return false;
    }

    return true;
}

void ESP32CertificateManager::updateCertificateMetadata() {
    // Update metadata for all installed certificates
    std::vector<CertificateType> types = {
        CertificateType::CentralSystemRootCert,
        CertificateType::ChargePointCert,
        CertificateType::ManufacturerRootCert,
        CertificateType::V2GRootCert,
        CertificateType::V2GCert};

    for (CertificateType type : types) {
        std::string certificate = getCertificate(type);
        if (!certificate.empty()) {
            loadCertificateMetadata(type);
        }
    }
}

SecurityConfig ESP32CertificateManager::createSecurityConfigProfile1() {
    SecurityConfig config;
    config.profile = SecurityProfile::Profile1_NoSecurity;
    return config;
}

SecurityConfig ESP32CertificateManager::createSecurityConfigProfile2(const std::string& serverName) {
    SecurityConfig config;
    config.profile = SecurityProfile::Profile2_TLS;
    config.verifyServerCertificate = true;
    config.verifyHostname = true;

    // Load CA certificate
    std::string caCert = getCertificate(CertificateType::CentralSystemRootCert);
    if (!caCert.empty()) {
        config.caCertificate = caCert;
    }

    if (!serverName.empty()) {
        config.sniServerName = serverName;
    }

    return config;
}

SecurityConfig ESP32CertificateManager::createSecurityConfigProfile3(const std::string& serverName) {
    SecurityConfig config = createSecurityConfigProfile2(serverName);
    config.profile = SecurityProfile::Profile3_TLS_Client;

    // Load client certificate and private key
    std::string clientCert = getCertificate(CertificateType::ChargePointCert);
    if (!clientCert.empty()) {
        config.clientCertificate = clientCert;
    }

    std::string privateKey = readSecureFile(getPrivateKeyPath(CertificateType::ChargePointCert));
    if (!privateKey.empty()) {
        config.clientPrivateKey = privateKey;
    }

    return config;
}

std::string ESP32CertificateManager::getLastErrorString() const {
    switch (lastError) {
        case CertificateManagerError::NONE:
            return "No error";
        case CertificateManagerError::FILESYSTEM_ERROR:
            return "Filesystem error";
        case CertificateManagerError::CERTIFICATE_PARSE_ERROR:
            return "Certificate parse error";
        case CertificateManagerError::KEY_PARSE_ERROR:
            return "Private key parse error";
        case CertificateManagerError::VALIDATION_FAILED:
            return "Certificate validation failed";
        case CertificateManagerError::STORAGE_FULL:
            return "Storage full";
        case CertificateManagerError::CERTIFICATE_EXPIRED:
            return "Certificate expired";
        case CertificateManagerError::KEY_MISMATCH:
            return "Key pair mismatch";
        case CertificateManagerError::UNSUPPORTED_FORMAT:
            return "Unsupported format";
        default:
            return "Unknown error";
    }
}

void ESP32CertificateManager::printDiagnostics() const {
    Serial.println();
    Serial.println("=== Certificate Manager Diagnostics ===");
    Serial.printf("Initialized: %s\n", initialized ? "Yes" : "No");
    Serial.printf("Filesystem Ready: %s\n", fileSystemReady ? "Yes" : "No");
    Serial.printf("Total Storage: %d bytes\n", static_cast<int>(LittleFS.totalBytes()));
    Serial.printf("Used Storage: %d bytes\n", static_cast<int>(LittleFS.usedBytes()));
    Serial.printf("Available Storage: %d bytes\n", static_cast<int>(getAvailableStorage()));
    Serial.printf("Certificate Storage Usage: %d bytes\n", static_cast<int>(getCertificateStorageUsage()));
    Serial.printf("Last Error: %s\n", getLastErrorString().c_str());

    Serial.println();
    Serial.println("Installed Certificates:");
    auto certs = getInstalledCertificates();
    for (const auto& cert : certs) {
        Serial.printf("  - %s\n", cert.c_str());
    }

    Serial.println("==================================");
}

std::unique_ptr<ESP32CertificateManager> ESP32CertificateManager::create() {
    auto manager = std::make_unique<ESP32CertificateManager>();

    if (!manager->initialize()) {
        Serial.println("Failed to create certificate manager");
        return nullptr;
    }

    return manager;
}
    if (!manager->initialize()) {
        Serial.println(\"Failed to create certificate manager\");
        return nullptr;
    }
    
    return manager;
}"