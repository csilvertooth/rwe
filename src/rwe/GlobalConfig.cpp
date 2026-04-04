#include "GlobalConfig.h"
#include <fstream>
#include <rwe/util/SimpleLogger.h>

namespace rwe
{
    void GlobalConfig::loadFromFile(const std::string& path)
    {
        settingsFilePath = path;
        std::ifstream file(path);
        if (!file.is_open())
        {
            LOG_INFO << "No config file found at " << path << ", using defaults";
            return;
        }

        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            auto eq = line.find('=');
            if (eq == std::string::npos)
            {
                continue;
            }

            auto key = line.substr(0, eq);
            auto value = line.substr(eq + 1);

            if (key == "windowWidth") windowWidth = std::stoi(value);
            else if (key == "windowHeight") windowHeight = std::stoi(value);
            else if (key == "fullscreen") fullscreen = (value == "1" || value == "true");
            else if (key == "fogOfWarEnabled") fogOfWarEnabled = (value == "1" || value == "true");
            else if (key == "trueLOS") trueLOS = (value == "1" || value == "true");
            else if (key == "healthBarsVisible") healthBarsVisible = (value == "1" || value == "true");
            else if (key == "soundVolume") soundVolume = std::stof(value);
            else if (key == "musicVolume") musicVolume = std::stof(value);
            else if (key == "soundEnabled") soundEnabled = (value == "1" || value == "true");
            else if (key == "musicEnabled") musicEnabled = (value == "1" || value == "true");
            else if (key == "leftClickInterfaceMode") leftClickInterfaceMode = (value == "1" || value == "true");
            else if (key == "scrollSpeed") scrollSpeed = std::stof(value);
            else if (key == "commanderEndsGame") commanderEndsGame = (value == "1" || value == "true");
            else if (key == "fixedPositions") fixedPositions = (value == "1" || value == "true");
            else if (key == "mappedTerrain") mappedTerrain = (value == "1" || value == "true");
            else if (key == "losMode") losMode = std::stoi(value);
            else if (key == "gameSpeed") gameSpeed = std::stof(value);
        }

        LOG_INFO << "Loaded config from " << path;
    }

    void GlobalConfig::saveToFile(const std::string& path) const
    {
        std::ofstream file(path);
        if (!file.is_open())
        {
            LOG_ERROR << "Failed to save config to " << path;
            return;
        }

        file << "# Annihilation Engine Configuration\n";
        file << "windowWidth=" << windowWidth << "\n";
        file << "windowHeight=" << windowHeight << "\n";
        file << "fullscreen=" << (fullscreen ? "1" : "0") << "\n";
        file << "fogOfWarEnabled=" << (fogOfWarEnabled ? "1" : "0") << "\n";
        file << "trueLOS=" << (trueLOS ? "1" : "0") << "\n";
        file << "healthBarsVisible=" << (healthBarsVisible ? "1" : "0") << "\n";
        file << "soundVolume=" << soundVolume << "\n";
        file << "musicVolume=" << musicVolume << "\n";
        file << "soundEnabled=" << (soundEnabled ? "1" : "0") << "\n";
        file << "musicEnabled=" << (musicEnabled ? "1" : "0") << "\n";
        file << "leftClickInterfaceMode=" << (leftClickInterfaceMode ? "1" : "0") << "\n";
        file << "scrollSpeed=" << scrollSpeed << "\n";
        file << "commanderEndsGame=" << (commanderEndsGame ? "1" : "0") << "\n";
        file << "fixedPositions=" << (fixedPositions ? "1" : "0") << "\n";
        file << "mappedTerrain=" << (mappedTerrain ? "1" : "0") << "\n";
        file << "losMode=" << losMode << "\n";
        file << "gameSpeed=" << gameSpeed << "\n";

        LOG_INFO << "Saved config to " << path;
    }
}
