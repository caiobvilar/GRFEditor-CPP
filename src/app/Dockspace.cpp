#include "app/Dockspace.h"

#include "app/ArchivePanel.h"

#include <algorithm>
#include <initializer_list>

#include <SDL3/SDL.h>
#include <SDL3/SDL_version.h>
#include <imgui.h>
#include <imgui_internal.h>

namespace grfeditor {
namespace Dockspace {

namespace {
constexpr const char* kVersion = "0.2.0 (M2: archive browser)";

bool gQuit = false;
bool gShowFileExplorer = true;
bool gShowPreview = true;
bool gShowProperties = true;
bool gShowStatusLog = true;
bool gShowToolbox = true;
bool gShowAbout = false;
bool gShowDemo = false;
bool gFirstFrame = true;

const ImVec4 kText = ImColor(0.90f, 0.92f, 0.95f, 1.0f);
const ImVec4 kTextDim = ImColor(0.45f, 0.50f, 0.57f, 1.0f);
const ImVec4 kAccent = ImColor(0.29f, 0.64f, 0.87f, 1.0f);
const ImVec4 kAccentDim = ImColor(0.20f, 0.38f, 0.55f, 1.0f);
} // namespace

void applyStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);

    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 1.0f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(6.0f, 5.0f);
    style.ItemInnerSpacing = ImVec2(5.0f, 5.0f);
    style.IndentSpacing = 18.0f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = kText;
    c[ImGuiCol_TextDisabled] = kTextDim;
    c[ImGuiCol_WindowBg] = ImColor(0.086f, 0.100f, 0.122f, 1.00f);
    c[ImGuiCol_ChildBg] = ImColor(0.055f, 0.065f, 0.082f, 1.00f);
    c[ImGuiCol_PopupBg] = ImColor(0.102f, 0.118f, 0.145f, 0.98f);
    c[ImGuiCol_Border] = ImColor(0.16f, 0.18f, 0.22f, 1.00f);
    c[ImGuiCol_BorderShadow] = ImColor(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg] = ImColor(0.13f, 0.15f, 0.18f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = kAccentDim;
    c[ImGuiCol_FrameBgActive] = kAccent;
    c[ImGuiCol_TitleBg] = ImColor(0.055f, 0.065f, 0.082f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImColor(0.12f, 0.16f, 0.20f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed] = ImColor(0.055f, 0.065f, 0.082f, 1.00f);
    c[ImGuiCol_MenuBarBg] = ImColor(0.10f, 0.12f, 0.14f, 1.00f);
    c[ImGuiCol_ScrollbarBg] = ImColor(0.09f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_ScrollbarGrab] = ImColor(0.20f, 0.24f, 0.29f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = kAccentDim;
    c[ImGuiCol_ScrollbarGrabActive] = kAccent;
    c[ImGuiCol_CheckMark] = kAccent;
    c[ImGuiCol_SliderGrab] = kAccentDim;
    c[ImGuiCol_SliderGrabActive] = kAccent;
    c[ImGuiCol_Button] = ImColor(0.13f, 0.15f, 0.18f, 1.00f);
    c[ImGuiCol_ButtonHovered] = kAccentDim;
    c[ImGuiCol_ButtonActive] = kAccent;
    c[ImGuiCol_Header] = ImColor(0.11f, 0.16f, 0.21f, 1.00f);
    c[ImGuiCol_HeaderHovered] = kAccentDim;
    c[ImGuiCol_HeaderActive] = kAccent;
    c[ImGuiCol_Separator] = ImColor(0.16f, 0.18f, 0.22f, 1.00f);
    c[ImGuiCol_SeparatorHovered] = kAccent;
    c[ImGuiCol_SeparatorActive] = kAccent;
    c[ImGuiCol_ResizeGrip] = ImColor(0.20f, 0.24f, 0.29f, 1.00f);
    c[ImGuiCol_ResizeGripHovered] = kAccent;
    c[ImGuiCol_ResizeGripActive] = kAccent;
    c[ImGuiCol_Tab] = ImColor(0.10f, 0.12f, 0.15f, 1.00f);
    c[ImGuiCol_TabHovered] = kAccentDim;
    c[ImGuiCol_TabSelected] = ImColor(0.16f, 0.25f, 0.35f, 1.00f);
    c[ImGuiCol_TabDimmed] = ImColor(0.08f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_TabDimmedSelected] = ImColor(0.12f, 0.18f, 0.24f, 1.00f);
    c[ImGuiCol_TextSelectedBg] = kAccentDim;
    c[ImGuiCol_NavHighlight] = kAccent;
    c[ImGuiCol_PlotHistogram] = kAccent;
    c[ImGuiCol_PlotHistogramHovered] = kAccent;
    c[ImGuiCol_DragDropTarget] = kAccent;
}

namespace {

void fileExplorerPane()
{
    if (!gShowFileExplorer)
        return;

    ImGui::Begin("File Explorer", &gShowFileExplorer);
    if (ImGui::Button("Open archive..."))
        ArchivePanel::get().requestOpenDialog();
    ImGui::SameLine();
    ImGui::TextDisabled("Ctrl+O");
    ImGui::Separator();
    ArchivePanel::get().renderFileExplorer();
    ImGui::End();
}

void previewPane()
{
    if (!gShowPreview)
        return;

    ImGui::Begin("Preview", &gShowPreview);
    ArchivePanel::get().renderPreviewPane();
    ImGui::End();
}

void propertiesPane()
{
    if (!gShowProperties)
        return;

    ImGui::Begin("Properties", &gShowProperties);
    ArchivePanel::get().renderPropertiesPane();
    ImGui::End();
}

void statusLogPane()
{
    if (!gShowStatusLog)
        return;

    ImGui::Begin("Status Log", &gShowStatusLog);
    ArchivePanel::get().renderStatusLogPane();
    ImGui::End();
}

void toolboxPane()
{
    if (!gShowToolbox)
        return;

    ImGui::Begin("Toolbox", &gShowToolbox);
    ImGui::TextDisabled("Planned tools:");
    ImGui::Separator();
    ImGui::BeginDisabled();
    ImGui::Selectable("Sprite Editor");
    ImGui::Selectable("Palette Editor");
    ImGui::Selectable("ACT Editor");
    ImGui::Selectable("STR Editor");
    ImGui::Selectable("Map Editor");
    ImGui::Selectable("Map Extractor");
    ImGui::Selectable("GRF Shrinker");
    ImGui::Selectable("GRF Validation");
    ImGui::Selectable("Image Converter");
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::TextDisabled("Available from M4/M5.");
    ImGui::End();
}

void aboutWindow()
{
    if (!gShowAbout)
        return;

    ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("About GRF Editor", &gShowAbout, ImGuiWindowFlags_NoDocking);
    ImGui::TextWrapped(
        "Cross-platform rewrite of the C# GRF Editor: an editor for the "
        "GRF, GPF and Thor archive formats from Ragnarok Online.");
    ImGui::Separator();
    ImGui::Text("Version");
    ImGui::SameLine(140.0f);
    ImGui::TextUnformatted(kVersion);
    ImGui::Text("Platform");
    ImGui::SameLine(140.0f);
    ImGui::TextUnformatted(SDL_GetPlatform());
    ImGui::Text("SDL3");
    ImGui::SameLine(140.0f);
    ImGui::Text("%d.%d.%d",
                SDL_VERSIONNUM_MAJOR(SDL_GetVersion()),
                SDL_VERSIONNUM_MINOR(SDL_GetVersion()),
                SDL_VERSIONNUM_MICRO(SDL_GetVersion()));
    ImGui::Text("Dear ImGui");
    ImGui::SameLine(140.0f);
    ImGui::TextUnformatted(IMGUI_VERSION);
    ImGui::Text("Graphics");
    ImGui::SameLine(140.0f);
    ImGui::TextUnformatted("OpenGL 3.3 Core (glad)");
    ImGui::Text("Toolchain");
    ImGui::SameLine(140.0f);
    ImGui::TextUnformatted("C++20 / Conan 2 / CMake");
    ImGui::End();
}

void setupFirstFrameLayout(ImGuiID dockspace_id)
{
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id,
                                  ImGui::GetMainViewport()->WorkSize);

    ImGuiID left, rest, right, right_top, right_bottom, center, status;
    ImGui::DockBuilderSplitNode(
        dockspace_id, ImGuiDir_Left, 0.22f, &left, &rest);
    ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.26f, &right, &center);
    ImGui::DockBuilderSplitNode(
        right, ImGuiDir_Up, 0.45f, &right_top, &right_bottom);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.22f, &status, &center);

    ImGui::DockBuilderDockWindow("File Explorer", left);
    ImGui::DockBuilderDockWindow("Preview", center);
    ImGui::DockBuilderDockWindow("Properties", right_top);
    ImGui::DockBuilderDockWindow("Toolbox", right_bottom);
    ImGui::DockBuilderDockWindow("Status Log", status);
    ImGui::DockBuilderFinish(dockspace_id);
}

void menuBar()
{
    ArchivePanel& panel = ArchivePanel::get();

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open archive...", "Ctrl+O"))
                panel.requestOpenDialog();
            if (ImGui::MenuItem(
                    "Save / Repack...", "Ctrl+S", false, !panel.readOnly()))
                panel.requestSaveDialog();
            if (ImGui::MenuItem(
                    "Save in place", nullptr, false, !panel.readOnly()))
                panel.saveInPlace();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
                gQuit = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
            ImGui::MenuItem("Redo", "Ctrl+Y", false, false);
            ImGui::Separator();
            if (ImGui::MenuItem(
                    "Add file...", nullptr, false, !panel.readOnly()))
                panel.requestAddFileDialog();
            if (ImGui::MenuItem("Extract selected...", nullptr))
                panel.extractSelected();
            if (ImGui::MenuItem(
                    "Remove selected", "Del", false, !panel.readOnly()))
                panel.removeSelected();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("File Explorer", nullptr, &gShowFileExplorer);
            ImGui::MenuItem("Preview", nullptr, &gShowPreview);
            ImGui::MenuItem("Properties", nullptr, &gShowProperties);
            ImGui::MenuItem("Status Log", nullptr, &gShowStatusLog);
            ImGui::MenuItem("Toolbox", nullptr, &gShowToolbox);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools"))
        {
            ImGui::MenuItem("Sprite Editor", nullptr, false, false);
            ImGui::MenuItem("ACT Editor", nullptr, false, false);
            ImGui::MenuItem("STR Editor", nullptr, false, false);
            ImGui::MenuItem("Map Editor", nullptr, false, false);
            ImGui::Separator();
            ImGui::MenuItem("GRF Shrinker", nullptr, false, false);
            if (ImGui::MenuItem("GRF Validation"))
                panel.runVerification();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("About", nullptr, &gShowAbout);
            ImGui::MenuItem("Dear ImGui Demo", nullptr, &gShowDemo);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void handleShortcuts()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
        return;
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false))
        ArchivePanel::get().requestOpenDialog();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
        ArchivePanel::get().saveInPlace();
}

} // namespace

bool render()
{
    gQuit = false;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGuiWindowFlags host_flags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    if (ImGui::Begin("##MainDockHost", nullptr, host_flags))
    {
        menuBar();
        handleShortcuts();

        constexpr float kStatusHeight = 24.0f;
        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        const ImVec2 content = ImGui::GetContentRegionAvail();
        ImGui::DockSpace(dockspace_id,
                         ImVec2(content.x, content.y - kStatusHeight));

        if (gFirstFrame)
        {
            setupFirstFrameLayout(dockspace_id);
            gFirstFrame = false;
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Ready");
        ImGui::SameLine();
        ArchivePanel& panel = ArchivePanel::get();
        std::string right;
        if (panel.hasArchive())
        {
            right = panel.path();
            if (right.size() > 80)
                right = "..." + right.substr(right.size() - 80);
            right += "  [" + std::string(panel.formatLabel()) +
                     (panel.readOnly() ? ", read-only" : "") + "]  " +
                     std::to_string(panel.entryCount()) + " entries";
        } else
        {
            right = "No GRF loaded";
        }
        ImGui::SetCursorPosX(content.x - ImGui::CalcTextSize(right.c_str()).x);
        ImGui::TextDisabled("%s", right.c_str());
    }
    ImGui::End();
    ImGui::PopStyleVar(3);

    fileExplorerPane();
    previewPane();
    propertiesPane();
    statusLogPane();
    toolboxPane();
    aboutWindow();
    ArchivePanel::get().renderDialogs();

    if (gShowDemo)
        ImGui::ShowDemoWindow(&gShowDemo);

    return gQuit;
}

} // namespace Dockspace
} // namespace grfeditor