#pragma once

namespace Core::Application {

class IIdGenerator {
public:
    virtual ~IIdGenerator() = default;
    virtual int nextTransactionId() = 0;
};

} // namespace Core::Application
