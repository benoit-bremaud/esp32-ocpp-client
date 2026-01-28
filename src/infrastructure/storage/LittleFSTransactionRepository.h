#pragma once

#include "../../../core/domain/ports/ITransactionRepository.h"
#include "../../../core/domain/ports/IFileSystem.h"
#include <string>

namespace Infrastructure {

class LittleFSTransactionRepository : public Core::Domain::ITransactionRepository {
private:
    Core::Domain::IFileSystem* fileSystem;
    std::string transactionPath;

    std::vector<Core::Domain::Transaction> loadAll();
    bool saveAll(const std::vector<Core::Domain::Transaction>& transactions);

public:
    LittleFSTransactionRepository(Core::Domain::IFileSystem* fs, const std::string& path);

    bool saveTransaction(const Core::Domain::Transaction& transaction) override;
    std::optional<Core::Domain::Transaction> getTransaction(int transactionId) override;
    std::vector<Core::Domain::Transaction> getActiveTransactions() override;
    bool removeTransaction(int transactionId) override;
    void clearAllTransactions() override;
};

} // namespace Infrastructure
