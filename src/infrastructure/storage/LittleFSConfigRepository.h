#pragma once

#include "../../../core/domain/ports/IConfigRepository.h"
#include "../../../core/domain/ports/IFileSystem.h"
#include <string>

namespace Infrastructure {

class LittleFSConfigRepository : public Core::Domain::IConfigRepository {
private:
    Core::Domain::IFileSystem* fileSystem;
    std::string configPath;

public:
    LittleFSConfigRepository(Core::Domain::IFileSystem* fs, const std::string& path);

    bool saveConfiguration(const Core::Domain::Configuration& config) override;
    Core::Domain::Configuration loadConfiguration() override;
    bool hasConfiguration() override;
    void resetToDefaults() override;
};

} // namespace Infrastructure
