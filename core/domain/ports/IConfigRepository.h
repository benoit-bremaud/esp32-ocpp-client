#pragma once

#include <string>
#include "../entities/Configuration.h"

namespace Core::Domain {

class IConfigRepository {
public:
    virtual ~IConfigRepository() = default;
    virtual bool saveConfiguration(const Configuration& config) = 0;
    virtual Configuration loadConfiguration() = 0;
    virtual bool hasConfiguration() = 0;
    virtual void resetToDefaults() = 0;
};

} // namespace Core::Domain
