#pragma once

#include <chrono>

namespace Core::Application {

class IClock {
public:
    virtual ~IClock() = default;
    virtual std::chrono::system_clock::time_point now() const = 0;
};

} // namespace Core::Application
