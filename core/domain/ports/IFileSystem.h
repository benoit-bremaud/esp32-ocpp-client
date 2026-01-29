#pragma once

#include <string>

namespace Core::Domain {

class IFileSystem {
public:
    virtual ~IFileSystem() = default;
    virtual bool writeFile(const std::string& path, const std::string& content) = 0;
    virtual std::string readFile(const std::string& path) = 0;
    virtual bool fileExists(const std::string& path) = 0;
    virtual bool deleteFile(const std::string& path) = 0;
    virtual bool initialize() = 0;
};

} // namespace Core::Domain
