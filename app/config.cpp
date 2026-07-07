#include "app/config.hpp"

#include "com/json.hpp"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <regex>
#include <sstream>

namespace Config {

Paths LoadPaths(const nlohmann::json& config)
{
    Paths paths{};
    if (config.contains("Paths"))
    {
        const auto& c = config["Paths"];
        paths.mShaders = c.value("Shaders", "");
        paths.mSaves = c.value("Saves", "");
        paths.mGameData = c.value("GameData", "");
        paths.mGraphicsOverrides = c.value("GraphicsOverrides", "");
        paths.mAssets4k = c.value("Assets4k", "");
        paths.mDialogMods = c.value("DialogMods", "");
        paths.mLuaMods = c.value("LuaMods", "");
    }
    return paths;
}

Graphics LoadGraphics(const nlohmann::json& config)
{
    Graphics graphics{};
    if (config.contains("Graphics"))
    {
        const auto& c = config["Graphics"];
        graphics.mResolutionScale = c.value("ResolutionScale", 4.0);
        graphics.mShadows = c.value("EnableShadows", true);
        graphics.mEnableImGui = c.value("EnableImGui", true);
        graphics.mDebugDisableFades = c.value("DebugDisableFades", false);
        graphics.mDebugRenderEncounters = c.value("DebugRenderEncounters", false);
        graphics.mDrawDistance = c.value("DrawDistance", 128000);
        graphics.mMaxTextureDim = c.value("MaxTextureDim", 2048);
        graphics.mMaxTextures = c.value("MaxTextures", 256);
        graphics.mRGBA8Upload = c.value("RGBA8Upload", false);
        graphics.mAsyncTextureUpload = c.value("AsyncTextureUpload", false);
        graphics.mAnisotropicFilter = c.value("AnisotropicFilter", 4.0);
        if (c.contains("HeroTextures"))
        {
            for (const auto& hero : c["HeroTextures"])
            {
                graphics.mHeroTextures.emplace_back(hero);
            }
        }
    }
    return graphics;
}

Logging LoadLogging(const nlohmann::json& config)
{
    Logging logging{};
    if (config.contains("Logging"))
    {
        const auto& c = config["Logging"];
        logging.mLogToFile = c.value("LogToFile", true);
        logging.mLogFilePath = c.value("LogFilePath", "");
        logging.mLogLevel = c.value("LogLevel", "Debug");
        if (c.contains("DisabledLoggers"))
        {
            for (const auto& logger : c["DisabledLoggers"])
            {
                logging.mDisabledLoggers.emplace_back(logger);
            }
        }
        if (c.contains("EnabledLoggers"))
        {
            for (const auto& logger : c["EnabledLoggers"])
            {
                logging.mEnabledLoggers.emplace_back(logger);
            }
        }
    }
    return logging;
}

Audio LoadAudio(const nlohmann::json& config)
{
    Audio audio{};
    if (config.contains("Audio"))
    {
        const auto& c = config["Audio"];
        audio.mEnableAudio = c.value("EnableAudio", true);
        audio.mEnableBackgroundSounds = c.value("EnableBackgroundSounds", true);
        audio.mMidiPlayer = c.value("MidiPlayer", "ADLMIDI");
    }
    return audio;
}

Game LoadGame(const nlohmann::json& config)
{
    Game game{};
    if (config.contains("Game"))
    {
        const auto& c = config["Game"];
        game.mAdvanceTime = c.value("AdvanceTime", true);
        game.mCombatSpeed = c.value("CombatSpeed", 1.0);
    }
    return game;
}


Config LoadConfig(std::string path)
{
    if (!std::filesystem::exists(path))
    {
        throw std::runtime_error("No config file at path: " + path);
    }

    std::ifstream in{};
    in.open(path, std::ios::in);

    const auto callback = nullptr;
    const bool allow_exceptions = true;
    const bool ignore_comments = true;
    auto data = nlohmann::json::parse(in, callback, allow_exceptions, ignore_comments);

    Config config{};
    config.mPaths = LoadPaths(data);
    config.mGraphics = LoadGraphics(data);
    config.mLogging = LoadLogging(data);
    config.mAudio = LoadAudio(data);
    config.mGame = LoadGame(data);

    std::cout << "Loaded config file: " << data <<"\n";

    return config;
}

bool SaveGraphicsValues(const std::string& configPath, const Graphics& g)
{
    if (configPath.empty() || !std::filesystem::exists(configPath))
    {
        std::cerr << "SaveGraphicsValues: config path not found: " << configPath << "\n";
        return false;
    }

    std::ifstream in{configPath, std::ios::in};
    if (!in.is_open())
    {
        std::cerr << "SaveGraphicsValues: could not open config for read: " << configPath << "\n";
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    in.close();
    std::string contents = ss.str();

    // Float formatter: shortest decimal that always keeps a decimal point, so an
    // integer-valued float still writes "4.0"/"0.0" (matches the style config.json
    // ships and round-trips as a float, not an int).
    const auto fmtFloat = [](float v) -> std::string
    {
        std::ostringstream os;
        os << v;
        std::string s = os.str();
        if (s.find('.') == std::string::npos) s += ".0";
        return s;
    };
    // Per-key value replace. Matches "Key" : <number> (number = int or float
    // literal) and rewrites just the number -- leading whitespace, the trailing
    // comma, and any inline comment survive because the regex does not consume
    // them. ECMAScript backref $1 = the captured key. Only the 3 owned keys are
    // touched; every other line (comments, formatting, other sections) is left
    // byte-identical so the extensive config.json comments are preserved.
    const auto replaceValue = [](std::string& s, const std::string& key, const std::string& val)
    {
        const std::regex re{"\"" + key + "\"\\s*:\\s*[0-9]+(?:\\.[0-9]+)?"};
        s = std::regex_replace(s, re, "\"" + key + "\": " + val);
    };

    replaceValue(contents, "ResolutionScale", fmtFloat(g.mResolutionScale));
    replaceValue(contents, "MaxTextureDim", std::to_string(g.mMaxTextureDim));
    replaceValue(contents, "AnisotropicFilter", fmtFloat(g.mAnisotropicFilter));

    std::ofstream out{configPath, std::ios::out | std::ios::trunc};
    if (!out.is_open())
    {
        std::cerr << "SaveGraphicsValues: could not open config for write: " << configPath << "\n";
        return false;
    }
    out << contents;
    out.close();
    return true;
}

}
