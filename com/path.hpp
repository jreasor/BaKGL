#pragma once

#include <filesystem>
#include <string>

std::string GetHomeDirectory();
class Paths
{
public:
    static Paths& Get();

    std::filesystem::path GetBakDirectoryPath() const;
    std::string GetBakDirectory() const;

    std::filesystem::path GetModDirectoryPath() const;
    std::string GetModDirectory() const;

    std::filesystem::path GetAssets4kDirectoryPath() const;
    std::string GetAssets4kDirectory() const;

    void SetBakDirectory(std::string path);
    void SetModDirectory(std::string path);
    void SetAssets4kDirectory(std::string path);
private:
    Paths();

    std::filesystem::path mBakDirectoryPath;
    std::filesystem::path mModDirectoryPath;
    std::filesystem::path mAssets4kDirectoryPath;
};


