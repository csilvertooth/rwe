#pragma once

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <string>

namespace rwe
{
    class RmlUiContext
    {
    public:
        RmlUiContext(SDL_Window* window, SDL_GLContext glContext);
        ~RmlUiContext();

        RmlUiContext(const RmlUiContext&) = delete;
        RmlUiContext& operator=(const RmlUiContext&) = delete;

        void loadFont(const std::string& path, bool fallback = false);
        Rml::ElementDocument* loadDocument(const std::string& path);
        void unloadDocument(Rml::ElementDocument* doc);

        void beginFrame();
        void render();

        bool processEvent(SDL_Event& event);
        void updateViewport(int width, int height);

        Rml::Context* getContext() { return context; }

    private:
        SDL_Window* window;
        Rml::Context* context{nullptr};
    };
}
