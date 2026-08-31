#pragma once

namespace grfeditor {

// Renders the editor shell: theme, menu bar, dock layout and placeholder panes.
namespace Dockspace {

// Applies the editor-wide ImGui style/theme.
void applyStyle();

// Renders one frame of the shell. Returns true when the user requests exit.
bool render();

} // namespace Dockspace
} // namespace grfeditor