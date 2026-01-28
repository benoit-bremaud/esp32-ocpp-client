#include "LittleFSTransactionRepository.h"
#include "../../../include/config.h"
#include <ArduinoJson.h>
#include <algorithm>

namespace Infrastructure {

namespace {
long long toEpochSeconds(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}

std::chrono::system_clock::time_point fromEpochSeconds(long long value) {
    return std::chrono::system_clock::time_point(std::chrono::seconds(value));
}
}

LittleFSTransactionRepository::LittleFSTransactionRepository(Core::Domain::IFileSystem* fs, const std::string& path)
    : fileSystem(fs), transactionPath(path.empty() ? TRANSACTION_FILE_PATH : path) {}

std::vector<Core::Domain::Transaction> LittleFSTransactionRepository::loadAll() {
    std::vector<Core::Domain::Transaction> transactions;
    if (!fileSystem || !fileSystem->fileExists(transactionPath)) {
        return transactions;
    }

    std::string content = fileSystem->readFile(transactionPath);
    if (content.empty()) {
        return transactions;
    }

    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, content)) {
        return transactions;
    }

    JsonArray array = doc.as<JsonArray>();
    for (JsonVariant item : array) {
        Core::Domain::Transaction tx;
        tx.transactionId = item["transactionId"] | 0;
        tx.connectorId = item["connectorId"] | 0;
        tx.idTag = item["idTag"].as<std::string>();
        tx.startMeterValue = item["startMeterValue"] | 0;
        tx.stopMeterValue = item["stopMeterValue"] | 0;
        tx.stopReason = item["stopReason"].as<std::string>();
        tx.isActive = item["isActive"] | false;

        long long startEpoch = item["startTime"] | 0LL;
        long long stopEpoch = item["stopTime"] | 0LL;
        tx.startTime = fromEpochSeconds(startEpoch);
        tx.stopTime = fromEpochSeconds(stopEpoch);

        transactions.push_back(tx);
    }

    return transactions;
}

bool LittleFSTransactionRepository::saveAll(const std::vector<Core::Domain::Transaction>& transactions) {
    if (!fileSystem) {
        return false;
    }

    DynamicJsonDocument doc(4096);
    JsonArray array = doc.to<JsonArray>();

    for (const auto& tx : transactions) {
        JsonObject obj = array.createNestedObject();
        obj["transactionId"] = tx.transactionId;
        obj["connectorId"] = tx.connectorId;
        obj["idTag"] = tx.idTag;
        obj["startMeterValue"] = tx.startMeterValue;
        obj["stopMeterValue"] = tx.stopMeterValue;
        obj["stopReason"] = tx.stopReason;
        obj["isActive"] = tx.isActive;
        obj["startTime"] = toEpochSeconds(tx.startTime);
        obj["stopTime"] = toEpochSeconds(tx.stopTime);
    }

    std::string output;
    serializeJson(doc, output);
    return fileSystem->writeFile(transactionPath, output);
}

bool LittleFSTransactionRepository::saveTransaction(const Core::Domain::Transaction& transaction) {
    auto transactions = loadAll();
    bool updated = false;

    for (auto& tx : transactions) {
        if (tx.transactionId == transaction.transactionId) {
            tx = transaction;
            updated = true;
            break;
        }
    }

    if (!updated) {
        transactions.push_back(transaction);
    }

    return saveAll(transactions);
}

std::optional<Core::Domain::Transaction> LittleFSTransactionRepository::getTransaction(int transactionId) {
    auto transactions = loadAll();
    for (const auto& tx : transactions) {
        if (tx.transactionId == transactionId) {
            return tx;
        }
    }
    return std::nullopt;
}

std::vector<Core::Domain::Transaction> LittleFSTransactionRepository::getActiveTransactions() {
    std::vector<Core::Domain::Transaction> active;
    auto transactions = loadAll();
    for (const auto& tx : transactions) {
        if (tx.isActive) {
            active.push_back(tx);
        }
    }
    return active;
}

bool LittleFSTransactionRepository::removeTransaction(int transactionId) {
    auto transactions = loadAll();
    auto before = transactions.size();
    transactions.erase(
        std::remove_if(transactions.begin(), transactions.end(),
                       [transactionId](const Core::Domain::Transaction& tx) {
                           return tx.transactionId == transactionId;
                       }),
        transactions.end());

    if (transactions.size() == before) {
        return false;
    }

    return saveAll(transactions);
}

void LittleFSTransactionRepository::clearAllTransactions() {
    if (!fileSystem) {
        return;
    }
    fileSystem->writeFile(transactionPath, "[]");
}

} // namespace Infrastructure
