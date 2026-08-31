#pragma once

#include "grfcore/Container.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace grfeditor {

// One node of the archive's virtual file system (directories + file leaves).
struct TreeNode
{
    std::string displayName; // UTF-8, for the UI
    std::string archivePath; // raw CP1252 archive bytes, backslash separated
    bool isDir = false;
    bool removed = false;
    std::vector<TreeNode> children;
    std::size_t entryIndex =
        static_cast<std::size_t>(-1); // leaf -> entries()[i]
};

// Owns the currently-opened archive and all the archive panels of the GUI
// (open dialog, virtual-filesystem tree, properties, preview, status log).
// Single-instance; Dockspace.cpp drives it.
class ArchivePanel
{
  public:
    static ArchivePanel& get();

    // --- archive lifecycle ---
    void openArchive(const std::string& path);
    bool hasArchive() const { return archive_.has_value(); }
    bool readOnly() const;
    const char* formatLabel() const;
    const std::string& path() const { return path_; }
    std::size_t entryCount() const;

    // --- actions (return false when they opened a modal instead) ---
    void requestOpenDialog();
    void requestSaveDialog();
    void requestAddFileDialog();
    void saveInPlace();
    void removeSelected();
    void extractSelected();
    void runVerification();

    // --- per-pane rendering ---
    void renderFileExplorer();
    void renderPropertiesPane();
    void renderPreviewPane();
    void renderStatusLogPane();
    void renderDialogs();

    // Status log access for the bottom status bar / other widgets.
    const std::string& lastStatus() const { return lastStatus_; }

  private:
    ArchivePanel() = default;

    void log(const std::string& line);
    void rebuildTree();
    grf::FileEntry* selectedEntry();
    const grf::FileEntry* selectedEntry() const;
    bool nodeVisible(const TreeNode& node,
                     const std::string& filterLower) const;
    void drawNode(const TreeNode& node, const std::string& filterLower);

    std::optional<grf::Container> archive_;
    std::string path_;
    TreeNode root_;
    std::string selectedPath_; // raw archive path of the selected leaf
    std::vector<std::string> logLines_;
    std::string lastStatus_;
    char filterBuf_[160] = {0};

    bool openDialogOpen_ = false;
    bool saveDialogOpen_ = false;
    bool addDialogOpen_ = false;
    char openPathBuf_[1024] = {0};
    char savePathBuf_[1024] = {0};
    char addTargetBuf_[1024] = {0};
    char addSourcePath_[1024] = {0};
};

} // namespace grfeditor