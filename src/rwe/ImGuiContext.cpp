#include "ImGuiContext.h"
#include <SDL3/SDL_events.h>

namespace rwe
{
    ImGuiContext::~ImGuiContext()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }

    bool wantsEvent(const ImGuiIO& io, const SDL_Event& event)
    {
        if (io.WantCaptureKeyboard)
        {
            if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_TEXT_INPUT)
            {
                return true;
            }
        }

        if (io.WantCaptureMouse)
        {
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP || event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_WHEEL)
            {
                return true;
            }
        }

        return false;
    }

    bool ImGuiContext::processEvent(const SDL_Event& event)
    {
        // Always forward events to ImGui so it can track hover state
        ImGui_ImplSDL3_ProcessEvent(&event);

        // Only consume the event (prevent scene from seeing it) if ImGui wants it
        if (wantsEvent(*io, event))
        {
            return true;
        }
        return false;
    }

    ImGuiContext::ImGuiContext(const std::string& iniPath, SDL_Window* window, void* glContext) : iniPath(iniPath)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        io = &ImGui::GetIO();
        io->IniFilename = this->iniPath.data();
        io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Use default ImGui font scaled for DPI (RmlUi handles styled menus)
        int winH = 0;
        SDL_GetWindowSize(window, nullptr, &winH);
        float dpiScale = std::max(1.0f, static_cast<float>(winH) / 768.0f);

        ImFontConfig fontConfig;
        fontConfig.SizePixels = 14.0f * dpiScale;
        fontConfig.OversampleH = 2;
        fontConfig.OversampleV = 2;
        io->Fonts->AddFontDefault(&fontConfig);
        io->FontGlobalScale = 1.0f;

        // Modern global style
        ImGui::StyleColorsDark();
        auto& style = ImGui::GetStyle();
        style.WindowRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.WindowPadding = ImVec2(14.0f, 14.0f);
        style.FramePadding = ImVec2(8.0f, 5.0f);
        style.ItemSpacing = ImVec2(8.0f, 8.0f);
        style.ScrollbarSize = 14.0f;

        // Clean dark color scheme
        auto* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.11f, 0.94f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.11f, 0.96f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.14f, 1.0f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.14f, 0.20f, 1.0f);
        colors[ImGuiCol_Header] = ImVec4(0.18f, 0.20f, 0.28f, 0.8f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.28f, 0.40f, 0.8f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.32f, 1.0f);
        colors[ImGuiCol_Button] = ImVec4(0.18f, 0.22f, 0.32f, 1.0f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.32f, 0.46f, 1.0f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.14f, 0.18f, 0.26f, 1.0f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.14f, 0.19f, 1.0f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.20f, 0.28f, 1.0f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.17f, 0.24f, 1.0f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.38f, 0.52f, 0.78f, 1.0f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.58f, 0.85f, 1.0f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.45f, 0.65f, 1.0f, 1.0f);
        colors[ImGuiCol_Separator] = ImVec4(0.22f, 0.24f, 0.30f, 0.6f);
        colors[ImGuiCol_Text] = ImVec4(0.90f, 0.92f, 0.96f, 1.0f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.48f, 0.55f, 1.0f);
        colors[ImGuiCol_Border] = ImVec4(0.20f, 0.22f, 0.28f, 0.5f);

        ImGui_ImplSDL3_InitForOpenGL(window, glContext);
        ImGui_ImplOpenGL3_Init("#version 150");
    }

    void ImGuiContext::newFrame(SDL_Window* window)
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiContext::render()
    {
        ImGui::Render();
    }

    void ImGuiContext::renderDrawData()
    {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}
