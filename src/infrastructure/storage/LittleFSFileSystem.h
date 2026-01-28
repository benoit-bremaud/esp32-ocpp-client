#pragma once

#include "../../../core/domain/ports/IFileSystem.h"
#include <string>

namespace Infrastructure {

class LittleFSFileSystem : public Core::Domain::IFileSystem {
public:
    bool writeFile(const std::string& path, const std::string& content) override;
    std::string readFile(const std::string& path) override;
    bool fileExists(const std::string& path) override;
    bool deleteFile(const std::string& path) override;
    bool initialize() override;
};

} // namespace Infrastructure
