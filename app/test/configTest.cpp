#include "app/config.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

namespace
{
    // RAII temp file: removed when the test ends, even on assertion failure, so
    // repeated/parallel runs don't leave crumbs in /tmp.
    struct TempConfig
    {
        std::string mPath;
        explicit TempConfig(std::string_view content)
            : mPath{(std::filesystem::temp_directory_path() / "bak_config_test.json").string()}
        {
            std::ofstream out{mPath, std::ios::out | std::ios::trunc};
            out << content;
        }
        ~TempConfig() { std::error_code ec; std::filesystem::remove(mPath, ec); }
        std::string read() const
        {
            std::ifstream in{mPath, std::ios::in};
            std::stringstream ss; ss << in.rdbuf();
            return ss.str();
        }
    };

    const auto countLines = [](const std::string& s)
    {
        return static_cast<long>(std::count(s.begin(), s.end(), '\n'));
    };

    // A realistic config.json slice: the 3 owned keys, a comment block, an
    // unowned key, and a non-Graphics section -- to prove SaveGraphicsValues
    // touches only the 3 owned numeric values and leaves everything else
    // (comments, formatting, other keys, other sections) byte-identical.
    const std::string kSample =
        "{\n"
        "  \"Graphics\": {\n"
        "    // the dynamic UI-scaling knob (Task 4.1)\n"
        "    \"ResolutionScale\": 4.0,\n"
        "    \"EnableShadows\": true,\n"
        "    // texture cap -- higher = more VRAM\n"
        "    \"MaxTextureDim\": 2048,\n"
        "    \"HeroTextures\": [\"A\", \"B\"],\n"
        "    // anisotropic filter level\n"
        "    \"AnisotropicFilter\": 4.0,\n"
        "    // Task 4.4: ON = original art, OFF = 4K (bool)\n"
        "    \"OriginalMode\": false\n"
        "  },\n"
        "  \"Logging\": {\n"
        "    \"LogLevel\": \"Debug\"\n"
        "  }\n"
        "}\n";
}

TEST(ConfigSaveGraphicsValues, RewritesOnlyTheThreeOwnedValues)
{
    TempConfig cfg{kSample};
    Config::Graphics g{};
    g.mResolutionScale = 5.4f;
    g.mMaxTextureDim = 4096;
    g.mAnisotropicFilter = 8.0f;
    ASSERT_TRUE(Config::SaveGraphicsValues(cfg.mPath, g));

    const auto out = cfg.read();
    EXPECT_NE(out.find("\"ResolutionScale\": 5.4,"), std::string::npos);
    EXPECT_NE(out.find("\"MaxTextureDim\": 4096,"), std::string::npos);
    EXPECT_NE(out.find("\"AnisotropicFilter\": 8.0"), std::string::npos);
    // the old values are gone
    EXPECT_EQ(out.find("\"ResolutionScale\": 4.0"), std::string::npos);
    EXPECT_EQ(out.find("\"MaxTextureDim\": 2048"), std::string::npos);
    EXPECT_EQ(out.find("\"AnisotropicFilter\": 4.0"), std::string::npos);
}

TEST(ConfigSaveGraphicsValues, PreservesCommentsAndUnownedKeys)
{
    TempConfig cfg{kSample};
    Config::Graphics g{};
    g.mResolutionScale = 10.8f;
    g.mMaxTextureDim = 1024;
    g.mAnisotropicFilter = 0.0f;
    ASSERT_TRUE(Config::SaveGraphicsValues(cfg.mPath, g));

    const auto out = cfg.read();
    // comments survive verbatim
    EXPECT_NE(out.find("// the dynamic UI-scaling knob (Task 4.1)"), std::string::npos);
    EXPECT_NE(out.find("// texture cap -- higher = more VRAM"), std::string::npos);
    EXPECT_NE(out.find("// anisotropic filter level"), std::string::npos);
    // unowned key + value untouched
    EXPECT_NE(out.find("\"EnableShadows\": true,"), std::string::npos);
    EXPECT_NE(out.find("\"HeroTextures\": [\"A\", \"B\"],"), std::string::npos);
    // other section untouched
    EXPECT_NE(out.find("\"LogLevel\": \"Debug\""), std::string::npos);
    // line count unchanged (only in-line values changed; no lines added/removed)
    EXPECT_EQ(countLines(out), countLines(kSample));
}

TEST(ConfigSaveGraphicsValues, AnisotropicZeroWritesDecimal)
{
    TempConfig cfg{kSample};
    Config::Graphics g{};
    g.mAnisotropicFilter = 0.0f;
    ASSERT_TRUE(Config::SaveGraphicsValues(cfg.mPath, g));
    EXPECT_NE(cfg.read().find("\"AnisotropicFilter\": 0.0"), std::string::npos);
}

// Task 4.4: the bool OriginalMode key persists via replaceBool (a sibling of
// replaceValue -- the numeric regex won't match true/false literals). Mirrors
// the numeric rewrite cases: only the matched bool literal changes; the key's
// comment, the 3 owned numeric values, and the line count are all preserved.
TEST(ConfigSaveGraphicsValues, RewritesOriginalModeBool)
{
    TempConfig cfg{kSample};
    Config::Graphics g{};
    g.mOriginalMode = true;
    ASSERT_TRUE(Config::SaveGraphicsValues(cfg.mPath, g));

    const auto out = cfg.read();
    EXPECT_NE(out.find("\"OriginalMode\": true"), std::string::npos);
    EXPECT_EQ(out.find("\"OriginalMode\": false"), std::string::npos);
    // the bool key's comment + the (unchanged) numeric values survive untouched
    EXPECT_NE(out.find("// Task 4.4: ON = original art, OFF = 4K (bool)"), std::string::npos);
    EXPECT_NE(out.find("\"AnisotropicFilter\": 4.0"), std::string::npos);
    // no lines added or removed -- only an in-line bool literal changed
    EXPECT_EQ(countLines(out), countLines(kSample));
}

TEST(ConfigSaveGraphicsValues, ReturnsFalseOnMissingPath)
{
    Config::Graphics g{};
    EXPECT_FALSE(Config::SaveGraphicsValues("/no/such/path/bak_config_test.json", g));
}