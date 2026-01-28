#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace Core::Domain {

struct SampledValue {
    std::string value;
    std::string context = "Sample.Periodic";
    std::string format = "Raw";
    std::string measurand = "Energy.Active.Import.Register";
    std::string phase;
    std::string location = "Outlet";
    std::string unit = "Wh";
};

struct MeterValue {
    std::chrono::system_clock::time_point timestamp{};
    std::vector<SampledValue> sampledValues;
};

} // namespace Core::Domain
