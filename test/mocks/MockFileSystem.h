#pragma once

#include "../../core/domain/ports/IFileSystem.h"
#include <map>
#include <string>

namespace MockInfrastructure {

class MockFileSystem : public Core::Domain::IFileSystem {
private:
    std::map<std::string, std::string> files;
    bool initialized = false;

public:
    bool writeFile(const std::string& path, const std::string& content) override {
        files[path] = content;
        return true;
    }

    std::string readFile(const std::string& path) override {
        auto it = files.find(path);
        return (it != files.end()) ? it->second : "";
    }

    bool fileExists(const std::string& path) override {
        return files.find(path) != files.end();
    }

    bool deleteFile(const std::string& path) override {
        return files.erase(path) > 0;
    }

    bool initialize() override {
        initialized = true;
        return true;
    }

    bool isInitialized() const { return initialized; }
    void clear() { files.clear(); }
};

} // namespace MockInfrastructure
