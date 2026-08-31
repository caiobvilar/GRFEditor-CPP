#include "app/Application.h"

#include <glad/gl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include "app/Dockspace.h"

namespace grfeditor {

namespace {
constexpr const char* kWindowTitle = "GRF Editor";
constexpr int kWindowWidth = 1600;
constexpr int kWindowHeight = 950;
constexpr ImVec4 kClearColor(0.043f, 0.051f, 0.067f, 1.0f);
} // namespace

int Application::run()
{
    Application app;
    if (!app.init())
        return 1;

    while (app.running_)
        app.frame();

    app.shutdown();
    return 0;
}

bool Application::init()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_Init failed: %s",
                     SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    window_ = SDL_CreateWindow(kWindowTitle,
                               kWindowWidth,
                               kWindowHeight,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                   SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window_ == nullptr)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateWindow failed: %s",
                     SDL_GetError());
        return false;
    }

    gl_context_ = SDL_GL_CreateContext(window_);
    if (gl_context_ == nullptr)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GL_CreateContext failed: %s",
                     SDL_GetError());
        return false;
    }

    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1);

    if (!gladLoadGL(static_cast<GLADloadfunc>(SDL_GL_GetProcAddress)))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "gladLoadGL failed");
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    Dockspace::applyStyle();

    if (!ImGui_ImplSDL3_InitForOpenGL(window_, gl_context_))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ImGui_ImplSDL3_InitForOpenGL failed");
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330"))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ImGui_ImplOpenGL3_Init failed");
        return false;
    }

    return true;
}

void Application::frame()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT)
            running_ = false;
    }

    if (!running_)
        return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (Dockspace::render())
        running_ = false;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::Render();
    glViewport(0,
               0,
               static_cast<int>(io.DisplaySize.x),
               static_cast<int>(io.DisplaySize.y));
    glClearColor(kClearColor.x, kClearColor.y, kClearColor.z, kClearColor.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        SDL_Window* backup_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext backup_context = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backup_window, backup_context);
    }

    SDL_GL_SwapWindow(window_);
}

void Application::shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (gl_context_ != nullptr)
        SDL_GL_DestroyContext(gl_context_);
    if (window_ != nullptr)
        SDL_DestroyWindow(window_);
    SDL_Quit();
}

} // namespace grfeditor