#define SDL_MAIN_HANDLED

#include <SDL3/SDL.h>

#include "app/Application.h"

int main(int, char**) { return grfeditor::Application::run(); }