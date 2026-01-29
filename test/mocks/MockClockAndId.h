#pragma once

#include "../../core/application/ports/IClock.h"
#include "../../core/application/ports/IIdGenerator.h"
#include <chrono>

namespace MockApplication {

class MockClock : public Core::Application::IClock {
private:
    std::chrono::system_clock::time_point current;

public:
    MockClock() : current(std::chrono::system_clock::now()) {}

    std::chrono::system_clock::time_point now() const override {
        return current;
    }

    void setNow(std::chrono::system_clock::time_point value) {
        current = value;
    }
};

class MockIdGenerator : public Core::Application::IIdGenerator {
private:
    int nextId = 1000;

public:
    explicit MockIdGenerator(int startId = 1000) : nextId(startId) {}

    int nextTransactionId() override {
        return nextId++;
    }
};

} // namespace MockApplication
