#pragma once

#include "../../../core/application/ports/IIdGenerator.h"

namespace Infrastructure {

class SimpleIdGenerator : public Core::Application::IIdGenerator {
private:
    int nextId = 1;

public:
    explicit SimpleIdGenerator(int startId = 1) : nextId(startId) {}

    int nextTransactionId() override {
        return nextId++;
    }
};

} // namespace Infrastructure
