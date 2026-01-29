#pragma once

#include <optional>
#include <vector>
#include "../entities/Transaction.h"

namespace Core::Domain {

class ITransactionRepository {
public:
    virtual ~ITransactionRepository() = default;
    virtual bool saveTransaction(const Transaction& transaction) = 0;
    virtual std::optional<Transaction> getTransaction(int transactionId) = 0;
    virtual std::vector<Transaction> getActiveTransactions() = 0;
    virtual bool removeTransaction(int transactionId) = 0;
    virtual void clearAllTransactions() = 0;
};

} // namespace Core::Domain
