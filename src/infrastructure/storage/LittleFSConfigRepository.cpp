#include "LittleFSConfigRepository.h"
#include "../../../include/config.h"
#include <ArduinoJson.h>

namespace Infrastructure {

LittleFSConfigRepository::LittleFSConfigRepository(Core::Domain::IFileSystem* fs, const std::string& path)
    : fileSystem(fs), configPath(path.empty() ? CONFIG_FILE_PATH : path) {}

bool LittleFSConfigRepository::saveConfiguration(const Core::Domain::Configuration& config) {
    if (!fileSystem) {
        return false;
    }

    JsonDocument doc;
    doc["centralSystemUrl"] = config.centralSystemUrl;
    doc["chargePointId"] = config.chargePointId;
    doc["chargePointPassword"] = config.chargePointPassword;
    doc["heartbeatInterval"] = config.heartbeatInterval;
    doc["meterValueSampleInterval"] = config.meterValueSampleInterval;
    doc["transactionMessageAttempts"] = config.transactionMessageAttempts;
    doc["transactionMessageRetryInterval"] = config.transactionMessageRetryInterval;
    doc["authorizeRemoteTxRequests"] = config.authorizeRemoteTxRequests;
    doc["localAuthorizeOffline"] = config.localAuthorizeOffline;
    doc["numberOfConnectors"] = config.numberOfConnectors;
    doc["supportedFeatureProfiles"] = config.supportedFeatureProfiles;
    doc["securityProfile"] = config.securityProfile;
    doc["cpoName"] = config.cpoName;
    doc["caCertificatePath"] = config.caCertificatePath;
    doc["clientCertificatePath"] = config.clientCertificatePath;
    doc["clientPrivateKeyPath"] = config.clientPrivateKeyPath;
    doc["verifyServerCertificate"] = config.verifyServerCertificate;
    doc["verifyHostname"] = config.verifyHostname;
    doc["tlsHandshakeTimeoutMs"] = config.tlsHandshakeTimeoutMs;

    JsonArray cipherSuites = doc["allowedCipherSuites"].to<JsonArray>();
    for (const auto& suite : config.allowedCipherSuites) {
        cipherSuites.add(suite);
    }

    if (doc.overflowed()) {
        return false;
    }

    std::string output;
    if (serializeJson(doc, output) == 0) {
        return false;
    }
    return fileSystem->writeFile(configPath, output);
}

Core::Domain::Configuration LittleFSConfigRepository::loadConfiguration() {
    Core::Domain::Configuration config;
    if (!fileSystem) {
        return config;
    }

    if (!fileSystem->fileExists(configPath)) {
        return config;
    }

    std::string content = fileSystem->readFile(configPath);
    if (content.empty()) {
        return config;
    }

    JsonDocument doc;
    auto err = deserializeJson(doc, content);
    if (err) {
        return config;
    }

    config.centralSystemUrl = doc["centralSystemUrl"].as<std::string>();
    config.chargePointId = doc["chargePointId"].as<std::string>();
    config.chargePointPassword = doc["chargePointPassword"].as<std::string>();
    config.heartbeatInterval = doc["heartbeatInterval"] | config.heartbeatInterval;
    config.meterValueSampleInterval = doc["meterValueSampleInterval"] | config.meterValueSampleInterval;
    config.transactionMessageAttempts = doc["transactionMessageAttempts"] | config.transactionMessageAttempts;
    config.transactionMessageRetryInterval = doc["transactionMessageRetryInterval"] | config.transactionMessageRetryInterval;
    config.authorizeRemoteTxRequests = doc["authorizeRemoteTxRequests"] | config.authorizeRemoteTxRequests;
    config.localAuthorizeOffline = doc["localAuthorizeOffline"] | config.localAuthorizeOffline;
    config.numberOfConnectors = doc["numberOfConnectors"] | config.numberOfConnectors;
    config.supportedFeatureProfiles = doc["supportedFeatureProfiles"] | config.supportedFeatureProfiles;
    config.securityProfile = doc["securityProfile"] | config.securityProfile;
    config.cpoName = doc["cpoName"] | config.cpoName;
    config.caCertificatePath = doc["caCertificatePath"] | config.caCertificatePath;
    config.clientCertificatePath = doc["clientCertificatePath"] | config.clientCertificatePath;
    config.clientPrivateKeyPath = doc["clientPrivateKeyPath"] | config.clientPrivateKeyPath;
    config.verifyServerCertificate = doc["verifyServerCertificate"] | config.verifyServerCertificate;
    config.verifyHostname = doc["verifyHostname"] | config.verifyHostname;
    config.tlsHandshakeTimeoutMs = doc["tlsHandshakeTimeoutMs"] | config.tlsHandshakeTimeoutMs;

    if (doc["allowedCipherSuites"].is<JsonArray>()) {
        config.allowedCipherSuites.clear();
        for (JsonVariant v : doc["allowedCipherSuites"].as<JsonArray>()) {
            config.allowedCipherSuites.push_back(v.as<std::string>());
        }
    }

    return config;
}

bool LittleFSConfigRepository::hasConfiguration() {
    if (!fileSystem) {
        return false;
    }
    return fileSystem->fileExists(configPath);
}

void LittleFSConfigRepository::resetToDefaults() {
    saveConfiguration(Core::Domain::Configuration());
}

} // namespace Infrastructure
