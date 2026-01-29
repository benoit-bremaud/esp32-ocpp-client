#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Core::Domain {

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

    // Security profile (OCPP 1.6-J)
    int securityProfile = 1; // 1=NoSecurity, 2=TLS, 3=TLS+ClientCert
    bool cpoName = false;

    // Certificate paths (relative to device FS)
    std::string caCertificatePath = "/certs/ca.pem";
    std::string clientCertificatePath = "/certs/client.pem";
    std::string clientPrivateKeyPath = "/certs/client.key";

    // TLS settings
    bool verifyServerCertificate = true;
    bool verifyHostname = true;
    std::vector<std::string> allowedCipherSuites;
    std::uint32_t tlsHandshakeTimeoutMs = 10000;
};

} // namespace Core::Domain
