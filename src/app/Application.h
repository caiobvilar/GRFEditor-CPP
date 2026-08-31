#pragma once

#include <SDL3/SDL.h>

namespace grfeditor {

// Owns the SDL window, the OpenGL context, the ImGui context and the main loop.
class Application
{
  public:
    static int run();

  private:
    bool init();
    void frame();
    void shutdown();

    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    bool running_ = true;
};

} // namespace grfeditor