#pragma once

#include "../../core/domain/ports/IConfigRepository.h"
#include "../../core/domain/ports/ITransactionRepository.h"
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace MockDomain {

class MockConfigRepository : public Core::Domain::IConfigRepository {
private:
    Core::Domain::Configuration config;
    bool hasConfig = false;

public:
    MockConfigRepository() {
        config.centralSystemUrl = "ws://test-server:8080/ocpp";
        config.chargePointId = "TEST_CP_001";
        config.heartbeatInterval = 60;
        config.meterValueSampleInterval = 300;
        hasConfig = true;
    }

    bool saveConfiguration(const Core::Domain::Configuration& newConfig) override {
        config = newConfig;
        hasConfig = true;
        return true;
    }

    Core::Domain::Configuration loadConfiguration() override {
        return config;
    }

    bool hasConfiguration() override {
        return hasConfig;
    }

    void resetToDefaults() override {
        config = Core::Domain::Configuration();
        hasConfig = true;
    }

    void setTestConfiguration(const Core::Domain::Configuration& testConfig) {
        config = testConfig;
        hasConfig = true;
    }
};

class MockTransactionRepository : public Core::Domain::ITransactionRepository {
private:
    std::map<int, Core::Domain::Transaction> transactions;

public:
    bool saveTransaction(const Core::Domain::Transaction& transaction) override {
        transactions[transaction.transactionId] = transaction;
        return true;
    }

    std::optional<Core::Domain::Transaction> getTransaction(int transactionId) override {
        auto it = transactions.find(transactionId);
        if (it != transactions.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::vector<Core::Domain::Transaction> getActiveTransactions() override {
        std::vector<Core::Domain::Transaction> active;
        for (const auto& kv : transactions) {
            if (kv.second.isActive) {
                active.push_back(kv.second);
            }
        }
        return active;
    }

    bool removeTransaction(int transactionId) override {
        return transactions.erase(transactionId) > 0;
    }

    void clearAllTransactions() override {
        transactions.clear();
    }

    void addTestTransaction(const Core::Domain::Transaction& transaction) {
        transactions[transaction.transactionId] = transaction;
    }

    size_t getTransactionCount() const {
        return transactions.size();
    }
};

} // namespace MockDomain
