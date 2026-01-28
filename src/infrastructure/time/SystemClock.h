#pragma once

#include "../../../core/application/ports/IClock.h"
#include <Arduino.h>
#include <chrono>

namespace Infrastructure {

class SystemClock : public Core::Application::IClock {
public:
    std::chrono::system_clock::time_point now() const override {
        auto ms = std::chrono::milliseconds(millis());
        return std::chrono::system_clock::time_point(ms);
    }
};

} // namespace Infrastructure
