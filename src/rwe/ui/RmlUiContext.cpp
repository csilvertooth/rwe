#include <GL/glew.h>
#include "RmlUiContext.h"
#include <RmlUi/Core.h>
#include <rwe/util/SimpleLogger.h>

// Include RmlUi SDL/GL3 backend implementations
#include <RmlUi_Platform_SDL.h>
#include <RmlUi_Renderer_GL3.h>

namespace rwe
{
    // Simple system interface using SDL timing
    class RweSystemInterface : public Rml::SystemInterface
    {
    public:
        double GetElapsedTime() override
        {
            return static_cast<double>(SDL_GetTicks()) / 1000.0;
        }

        bool LogMessage(Rml::Log::Type type, const Rml::String& message) override
        {
            switch (type)
            {
                case Rml::Log::LT_ERROR:
                case Rml::Log::LT_ASSERT:
                    LOG_ERROR << "RmlUi: " << message;
                    break;
                case Rml::Log::LT_WARNING:
                    LOG_INFO << "RmlUi: " << message;
                    break;
                default:
                    break;
            }
            return true;
        }
    };

    static RweSystemInterface systemInterface;
    static std::unique_ptr<RenderInterface_GL3> renderInterface;

    RmlUiContext::RmlUiContext(SDL_Window* window, SDL_GLContext /*glContext*/)
        : window(window)
    {
        // Initialize GL3 render interface
        renderInterface = std::make_unique<RenderInterface_GL3>();

        // Set interfaces before Rml::Initialise
        Rml::SetSystemInterface(&systemInterface);
        Rml::SetRenderInterface(renderInterface.get());

        if (!Rml::Initialise())
        {
            LOG_ERROR << "Failed to initialize RmlUi";
            return;
        }

        // Use logical window size for RmlUi context (CSS pixels)
        int logW, logH;
        SDL_GetWindowSize(window, &logW, &logH);

        // Set pixel viewport on renderer for proper DPI rendering
        int pixW, pixH;
        SDL_GetWindowSizeInPixels(window, &pixW, &pixH);
        renderInterface->SetViewport(pixW, pixH);

        context = Rml::CreateContext("main", Rml::Vector2i(logW, logH));
        if (!context)
        {
            LOG_ERROR << "Failed to create RmlUi context";
            return;
        }

        // Set DPI scaling ratio
        float dpRatio = static_cast<float>(pixW) / static_cast<float>(logW);
        context->SetDensityIndependentPixelRatio(dpRatio);

        LOG_INFO << "RmlUi initialized (" << logW << "x" << logH << " @ " << dpRatio << "x DPI)";
    }

    RmlUiContext::~RmlUiContext()
    {
        if (context)
        {
            Rml::RemoveContext(context->GetName());
        }
        Rml::Shutdown();
        renderInterface.reset();
    }

    void RmlUiContext::loadFont(const std::string& path, bool fallback)
    {
        if (!Rml::LoadFontFace(path, fallback))
        {
            LOG_ERROR << "Failed to load font: " << path;
        }
        else
        {
            LOG_INFO << "Loaded font: " << path;
        }
    }

    Rml::ElementDocument* RmlUiContext::loadDocument(const std::string& path)
    {
        if (!context) return nullptr;
        auto* doc = context->LoadDocument(path);
        if (doc)
        {
            doc->Show();
            LOG_INFO << "Loaded RML document: " << path;
        }
        else
        {
            LOG_ERROR << "Failed to load RML document: " << path;
        }
        return doc;
    }

    void RmlUiContext::unloadDocument(Rml::ElementDocument* doc)
    {
        if (context && doc)
        {
            context->UnloadDocument(doc);
        }
    }

    void RmlUiContext::beginFrame()
    {
        if (!context) return;
        if (context->GetNumDocuments() == 0) return;
        context->Update();
    }

    void RmlUiContext::render()
    {
        if (!context) return;
        if (context->GetNumDocuments() == 0) return;

        // Save GL state that the game uses
        GLint prevFramebuffer;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFramebuffer);
        GLboolean prevDepthTest = glIsEnabled(GL_DEPTH_TEST);
        GLboolean prevCullFace = glIsEnabled(GL_CULL_FACE);
        GLboolean prevBlend = glIsEnabled(GL_BLEND);

        // RmlUi needs default framebuffer and specific GL state
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        renderInterface->BeginFrame();
        context->Render();
        renderInterface->EndFrame();

        // Restore GL state
        glBindFramebuffer(GL_FRAMEBUFFER, prevFramebuffer);
        if (prevDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (prevCullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        if (prevBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    }

    bool RmlUiContext::processEvent(SDL_Event& event)
    {
        if (!context) return false;
        // Only consume events when documents are loaded and visible
        if (context->GetNumDocuments() == 0) return false;
        return RmlSDL::InputEventHandler(context, window, event);
    }

    void RmlUiContext::updateViewport(int width, int height)
    {
        if (!context) return;
        // width/height are pixel dimensions from SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
        renderInterface->SetViewport(width, height);

        // Get logical size for RmlUi context
        int logW, logH;
        SDL_GetWindowSize(window, &logW, &logH);
        context->SetDimensions(Rml::Vector2i(logW, logH));

        float dpRatio = (logW > 0) ? static_cast<float>(width) / static_cast<float>(logW) : 1.0f;
        context->SetDensityIndependentPixelRatio(dpRatio);
    }
}
