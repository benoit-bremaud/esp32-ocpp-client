#include "LittleFSFileSystem.h"
#include <LittleFS.h>

namespace Infrastructure {

bool LittleFSFileSystem::initialize() {
    return LittleFS.begin(false);
}

bool LittleFSFileSystem::writeFile(const std::string& path, const std::string& content) {
    File file = LittleFS.open(path.c_str(), "w");
    if (!file) {
        return false;
    }
    file.print(content.c_str());
    file.close();
    return true;
}

std::string LittleFSFileSystem::readFile(const std::string& path) {
    File file = LittleFS.open(path.c_str(), "r");
    if (!file) {
        return "";
    }

    std::string content;
    while (file.available()) {
        content.push_back(static_cast<char>(file.read()));
    }
    file.close();
    return content;
}

bool LittleFSFileSystem::fileExists(const std::string& path) {
    return LittleFS.exists(path.c_str());
}

bool LittleFSFileSystem::deleteFile(const std::string& path) {
    return LittleFS.remove(path.c_str());
}

} // namespace Infrastructure
