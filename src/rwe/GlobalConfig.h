#pragma once

#include <string>

namespace rwe
{
    class GlobalConfig
    {
    public:
        // Interface
        bool leftClickInterfaceMode{true};
        float scrollSpeed{1.0f};

        // Display
        unsigned int windowWidth{1280};
        unsigned int windowHeight{960};
        bool fullscreen{false};

        // Gameplay
        bool fogOfWarEnabled{true};
        bool trueLOS{false};
        bool healthBarsVisible{false};
        float gameSpeed{1.0f};

        // Sound
        float soundVolume{1.0f};
        float musicVolume{0.5f};
        bool soundEnabled{true};
        bool musicEnabled{true};

        // Skirmish defaults
        bool commanderEndsGame{true};
        bool fixedPositions{true};
        bool mappedTerrain{false}; // false = unmapped (fog of war black)
        int losMode{0}; // 0=circular, 1=true, 2=permanent

        std::string settingsFilePath;

        void loadFromFile(const std::string& path);
        void saveToFile(const std::string& path) const;
        void save() const { if (!settingsFilePath.empty()) saveToFile(settingsFilePath); }
    };
}
