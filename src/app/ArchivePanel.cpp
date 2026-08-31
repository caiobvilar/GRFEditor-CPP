#include "app/ArchivePanel.h"

#include "grfcore/Compression.h"
#include "grfcore/GrfContainerProvider.h"
#include "grfcore/GrfUtil.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>

#include <imgui.h>

namespace fs = std::filesystem;
using namespace grf;

namespace grfeditor {

namespace {

bool looksLikeText(const Bytes& data)
{
    std::size_t nonPrintable = 0;
    for (std::size_t i = 0; i < data.size() && i < 4096; ++i)
    {
        byte c = data[i];
        if (c == 0 || (c < 0x20 && c != '\n' && c != '\r' && c != '\t'))
            nonPrintable++;
    }
    return data.empty() ||
           (nonPrintable * 100 /
                std::max<std::size_t>(
                    1, std::min<std::size_t>(data.size(), 4096)) <
            10);
}

// Blocks on a zenity file dialog (Linux). Returns false when zenity is
// unavailable or the user cancelled.
bool nativeFileDialog(bool save,
                      std::string& result,
                      const std::string& defaultName = "")
{
    std::string cmd = save
                          ? "zenity --file-selection --save --confirm-overwrite"
                          : "zenity --file-selection";
    if (!defaultName.empty())
        cmd += " --filename=\"" + defaultName + "\"";
    FILE* p = popen(cmd.c_str(), "r");
    if (!p)
        return false;
    char buf[4096];
    std::string res;
    while (fgets(buf, sizeof buf, p) != nullptr)
        res += buf;
    int rc = pclose(p);
    while (!res.empty() && (res.back() == '\n' || res.back() == '\r'))
        res.pop_back();
    if (rc != 0 || res.empty())
        return false;
    result = res;
    return true;
}

std::vector<std::string> splitBackslash(const std::string& raw)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= raw.size())
    {
        std::size_t pos = raw.find('\\', start);
        if (pos == std::string::npos)
        {
            parts.push_back(raw.substr(start));
            break;
        }
        parts.push_back(raw.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

std::string lowerUtf8Ascii(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
    return s;
}

void centeredMessageR(const char* title, const char* subtitle)
{
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 ts = ImGui::CalcTextSize(title);
    const ImVec2 ss = ImGui::CalcTextSize(subtitle);
    ImGui::Dummy(
        ImVec2(0.0f, std::max(0.0f, (avail.y - ts.y - ss.y - 6.0f) * 0.5f)));
    ImGui::SetCursorPosX(std::max(0.0f, (avail.x - ts.x) * 0.5f));
    ImGui::TextUnformatted(title);
    ImGui::SetCursorPosX(std::max(0.0f, (avail.x - ss.x) * 0.5f));
    ImGui::TextDisabled("%s", subtitle);
}

const char* flagBits(std::uint32_t flags)
{
    static thread_local std::string text;
    const char* kNames[] = {"hdr-crypt",
                            "data-crypt",
                            "remove",
                            "editor-crypt",
                            "gravity-enc",
                            "decrypt",
                            "renamed",
                            "lzma",
                            "raw",
                            "lzss"};
    static const std::uint32_t kBits[] = {entry_type::HeaderCrypted,
                                          entry_type::DataCrypted,
                                          entry_type::RemoveFile,
                                          entry_type::GrfEditorCrypted,
                                          entry_type::GravityEncryptedFile,
                                          entry_type::Decrypt,
                                          entry_type::FileNameRenamed,
                                          entry_type::LzmaCompressed,
                                          entry_type::RawDataFile,
                                          entry_type::LZSS};
    text.clear();
    for (int i = 0; i < 10; ++i)
        if ((flags & kBits[i]) != 0)
        {
            if (!text.empty())
                text += ", ";
            text += kNames[i];
        }
    return text.c_str();
}

} // namespace

ArchivePanel& ArchivePanel::get()
{
    static ArchivePanel instance;
    return instance;
}

bool ArchivePanel::readOnly() const
{
    return archive_.has_value() && archive_->readOnly();
}

const char* ArchivePanel::formatLabel() const
{
    return archive_.has_value() ? archive_->formatLabel() : "-";
}

std::size_t ArchivePanel::entryCount() const
{
    return archive_.has_value() ? archive_->entries().size() : 0;
}

void ArchivePanel::log(const std::string& line)
{
    logLines_.push_back(line);
    lastStatus_ = line;
    // Trim ancient history.
    if (logLines_.size() > 300)
        logLines_.erase(logLines_.begin(),
                        logLines_.begin() + (logLines_.size() - 300));
}

void ArchivePanel::openArchive(const std::string& path)
{
    try
    {
        archive_.emplace(openContainer(path));
        path_ = path;
        selectedPath_.clear();
        root_.children.clear();
        rebuildTree();
        std::ostringstream ss;
        ss << "Opened " << path << " (" << formatLabel() << ", " << entryCount()
           << " entries";
        if (readOnly())
            ss << ", read-only";
        ss << ")";
        log(ss.str());
    } catch (const std::exception& ex)
    {
        archive_.reset();
        log("Open failed: " + std::string(ex.what()));
    }
}

void ArchivePanel::rebuildTree()
{
    if (!archive_)
        return;
    root_ = TreeNode{};
    root_.isDir = true;
    const auto& entries = archive_->entries();

    // Pass 1: create directories.
    for (const auto& e : entries)
    {
        auto parts = splitBackslash(e.relativePath);
        if (parts.size() <= 1)
            continue;
        TreeNode* cur = &root_;
        std::string arch;
        for (std::size_t i = 0; i + 1 < parts.size(); ++i)
        {
            if (!arch.empty())
                arch += '\\';
            arch += parts[i];
            TreeNode* next = nullptr;
            for (auto& ch : cur->children)
                if (ch.isDir && ch.archivePath == arch)
                {
                    next = &ch;
                    break;
                }
            if (!next)
            {
                cur->children.push_back(TreeNode{cp1252ToUtf8(parts[i]),
                                                 arch,
                                                 true,
                                                 false,
                                                 {},
                                                 static_cast<std::size_t>(-1)});
                next = &cur->children.back();
            }
            cur = next;
        }
    }

    // Pass 2: file leaves (keep insertion order for stability).
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries[i];
        auto parts = splitBackslash(e.relativePath);
        TreeNode* cur = &root_;
        if (parts.size() > 1)
        {
            std::string arch;
            for (std::size_t j = 0; j + 1 < parts.size(); ++j)
            {
                arch += (j ? "\\" : "") + parts[j];
                for (auto& ch : cur->children)
                    if (ch.isDir && ch.archivePath == arch)
                    {
                        cur = &ch;
                        break;
                    }
            }
        }
        cur->children.push_back(
            TreeNode{cp1252ToUtf8(parts.back()),
                     e.relativePath,
                     false,
                     (e.flags & entry_type::RemoveFile) != 0,
                     {},
                     i});
    }
}

grf::FileEntry* ArchivePanel::selectedEntry()
{
    if (!archive_ || selectedPath_.empty())
        return nullptr;
    return archive_->find(selectedPath_);
}

const grf::FileEntry* ArchivePanel::selectedEntry() const
{
    if (!archive_ || selectedPath_.empty())
        return nullptr;
    return archive_->find(selectedPath_);
}

bool ArchivePanel::nodeVisible(const TreeNode& node,
                               const std::string& filterLower) const
{
    if (filterLower.empty())
        return true;
    if (!node.isDir)
        return lowerUtf8Ascii(node.displayName).find(filterLower) !=
               std::string::npos;
    for (const auto& c : node.children)
        if (nodeVisible(c, filterLower))
            return true;
    return false;
}

void ArchivePanel::drawNode(const TreeNode& node,
                            const std::string& filterLower)
{
    for (const auto& child : node.children)
    {
        if (!nodeVisible(child, filterLower))
            continue;
        if (child.isDir)
        {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                       ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                       ImGuiTreeNodeFlags_SpanAvailWidth |
                                       ImGuiTreeNodeFlags_FramePadding;
            if (child.displayName == "data" || child.displayName == "root" ||
                child.displayName == "system")
                flags |= ImGuiTreeNodeFlags_DefaultOpen;
            bool open = ImGui::TreeNodeEx((void*)(intptr_t)&child,
                                          flags,
                                          "%s",
                                          child.displayName.c_str());
            if (open)
            {
                drawNode(child, filterLower);
                ImGui::TreePop();
            }
        } else
        {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
                                       ImGuiTreeNodeFlags_SpanAvailWidth |
                                       ImGuiTreeNodeFlags_FramePadding;
            if (selectedPath_ == child.archivePath)
                flags |= ImGuiTreeNodeFlags_Selected;
            ImGui::TreeNodeEx((void*)(intptr_t)&child,
                              flags,
                              "%s%s",
                              child.removed ? "[x] " : "",
                              child.displayName.c_str());
            if (ImGui::IsItemClicked())
                selectedPath_ = child.archivePath;
            ImGui::TreePop();
        }
    }
}

// ---------------------------------------------------------------------------
// Panes
// ---------------------------------------------------------------------------

void ArchivePanel::renderFileExplorer()
{
    if (!hasArchive())
    {
        centeredMessageR("No archive open",
                         "Use File > Open (Ctrl+O) or drop a .grf / .gpf / "
                         ".thor / .rgz path.");
        return;
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint(
        "##archivefilter", "Filter...", filterBuf_, sizeof filterBuf_);
    ImGui::Separator();

    const std::string filterLower = lowerUtf8Ascii(filterBuf_);
    if (ImGui::BeginChild("##treelist"))
    {
        ImGui::PushStyleColor(ImGuiCol_Header,
                              ImVec4(0.29f, 0.33f, 0.40f, 0.6f));
        drawNode(root_, filterLower);
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
}

void ArchivePanel::renderPropertiesPane()
{
    const grf::FileEntry* e = selectedEntry();
    if (!e)
    {
        centeredMessageR("No entry selected",
                         "Select a file in the File Explorer tree.");
        return;
    }

    ImGui::Columns(2, nullptr, false);
    ImGui::TextDisabled("Name");
    ImGui::NextColumn();
    ImGui::TextUnformatted(cp1252ToUtf8(e->relativePath).c_str());
    ImGui::NextColumn();
    ImGui::TextDisabled("Size on disk");
    ImGui::NextColumn();
    ImGui::Text("%d bytes", e->sizeCompressedAlignment);
    ImGui::NextColumn();
    ImGui::TextDisabled("Compressed");
    ImGui::NextColumn();
    ImGui::Text("%d bytes", e->sizeCompressed);
    ImGui::NextColumn();
    ImGui::TextDisabled("Decompressed");
    ImGui::NextColumn();
    ImGui::Text("%d bytes", e->sizeDecompressed);
    ImGui::NextColumn();
    ImGui::TextDisabled("Flags");
    ImGui::NextColumn();
    ImGui::TextUnformatted(flagBits(e->flags));
    ImGui::NextColumn();
    ImGui::TextDisabled("Offset");
    ImGui::NextColumn();
    ImGui::Text("%lld", (long long)e->fileExactOffset);
    ImGui::NextColumn();
    ImGui::Columns(1);
    ImGui::Separator();

    const char* fmt = readOnly() ? formatLabel() : "GRF";
    if (ImGui::Button("Extract..."))
        extractSelected();
    ImGui::SameLine();
    if (!readOnly())
    {
        if (ImGui::Button("Remove"))
            removeSelected();
    } else
    {
        ImGui::TextDisabled("read-only archive (%s)", fmt);
    }
}

void ArchivePanel::renderPreviewPane()
{
    const grf::FileEntry* e = selectedEntry();
    if (!e)
    {
        centeredMessageR("No preview",
                         "Select an entry to preview its content.");
        return;
    }

    Bytes data;
    try
    {
        data = archive_->extract(*const_cast<grf::FileEntry*>(e));
    } catch (const std::exception& ex)
    {
        centeredMessageR("Preview failed", ex.what());
        return;
    }

    ImGui::TextUnformatted(cp1252ToUtf8(e->relativePath).c_str());
    ImGui::TextDisabled("%zu bytes", data.size());
    ImGui::Separator();

    if (data.size() > 1024 * 1024)
        ImGui::TextWrapped("File is too large to preview (%.1f MB); using the "
                           "Extract button instead.",
                           data.size() / (1024.0 * 1024.0));
    else if (looksLikeText(data))
    {
        if (ImGui::BeginChild("##textpreview",
                              ImVec2(0, 0),
                              false,
                              ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImGui::TextWrapped(
                "%.*s", (int)data.size(), (const char*)data.data());
        }
        ImGui::EndChild();
    } else
    {
        ImGui::TextWrapped("Binary file (%zu bytes): a raw preview is not "
                           "rendered yet (sprites/palettes land in M3).",
                           data.size());
        ImGui::Separator();
        constexpr std::size_t kHex = 384;
        std::size_t shown = std::min<std::size_t>(data.size(), kHex);
        std::string hex;
        for (std::size_t i = 0; i < shown; ++i)
        {
            char b[8];
            std::snprintf(b, sizeof b, "%02x ", data[i]);
            hex += b;
            if ((i + 1) % 16 == 0)
                hex += '\n';
        }
        if (ImGui::BeginChild("##hexpreview",
                              ImVec2(0, 0),
                              false,
                              ImGuiWindowFlags_HorizontalScrollbar))
            ImGui::TextUnformatted(hex.c_str());
        ImGui::EndChild();
    }
}

void ArchivePanel::renderStatusLogPane()
{
    if (ImGui::BeginChild("##statuslog"))
    {
        if (logLines_.empty())
            ImGui::TextDisabled("Nothing logged yet.");
        for (const auto& line : logLines_)
            ImGui::TextUnformatted(line.c_str());
        if (!logLines_.empty())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void ArchivePanel::requestOpenDialog()
{
    std::snprintf(openPathBuf_, sizeof openPathBuf_, "%s", path_.c_str());
    openDialogOpen_ = true;
}

void ArchivePanel::requestSaveDialog()
{
    if (readOnly())
    {
        log("Save not available: THOR/RGZ archives are read-only.");
        return;
    }
    std::snprintf(savePathBuf_, sizeof savePathBuf_, "%s", path_.c_str());
    saveDialogOpen_ = true;
}

void ArchivePanel::requestAddFileDialog()
{
    if (readOnly())
    {
        log("Add not available: THOR/RGZ archives are read-only.");
        return;
    }
    addSourcePath_[0] = '\0';
    const grf::FileEntry* e = selectedEntry();
    if (e)
    {
        auto parts = splitBackslash(e->relativePath);
        parts.pop_back();
        std::string dir;
        for (const auto& p : parts)
            dir += p + "\\";
        std::snprintf(
            addTargetBuf_, sizeof addTargetBuf_, "%s<file name>", dir.c_str());
    } else
    {
        addTargetBuf_[0] = '\0';
    }
    addDialogOpen_ = true;
}

void ArchivePanel::saveInPlace()
{
    if (!archive_)
    {
        log("No archive open.");
        return;
    }
    if (readOnly())
    {
        log("Save not available: THOR/RGZ archives are read-only.");
        return;
    }
    try
    {
        archive_->save(path_);
        rebuildTree();
        log("Saved (in place): " + path_);
    } catch (const std::exception& ex)
    {
        log("Save failed: " + std::string(ex.what()));
    }
}

void ArchivePanel::removeSelected()
{
    if (!archive_ || readOnly())
        return;
    grf::FileEntry* e = selectedEntry();
    if (!e)
    {
        log("Nothing selected to remove.");
        return;
    }
    try
    {
        archive_->removeEntry(*e);
        rebuildTree();
        std::string removed = selectedPath_;
        selectedPath_.clear();
        log("Removed: " + cp1252ToUtf8(removed));
    } catch (const std::exception& ex)
    {
        log("Remove failed: " + std::string(ex.what()));
    }
}

void ArchivePanel::extractSelected()
{
    const grf::FileEntry* e = selectedEntry();
    if (!e)
    {
        log("Nothing selected to extract.");
        return;
    }

    Bytes data;
    try
    {
        data = archive_->extract(*const_cast<grf::FileEntry*>(e));
    } catch (const std::exception& ex)
    {
        log("Extract failed: " + std::string(ex.what()));
        return;
    }

    auto parts = splitBackslash(e->relativePath);
    const std::string defaultName =
        parts.empty() ? "extracted.bin" : parts.back();
    std::string dest;
    if (nativeFileDialog(true, dest, defaultName))
    {
        try
        {
            writeFile(dest, data.data(), data.size());
            log("Extracted " + cp1252ToUtf8(e->relativePath) + " -> " + dest +
                " (" + std::to_string(data.size()) + " bytes)");
        } catch (const std::exception& ex)
        {
            log("Extract failed: " + std::string(ex.what()));
        }
    }
}

void ArchivePanel::runVerification()
{
    if (!archive_)
    {
        log("No archive open.");
        return;
    }
    std::size_t ok = 0, fail = 0;
    std::vector<std::string> failures;
    for (const auto& e : archive_->entries())
    {
        if ((e.flags & entry_type::RemoveFile) != 0)
            continue;
        try
        {
            Bytes data = archive_->extract(const_cast<grf::FileEntry&>(e));
            if (data.size() != (std::size_t)e.sizeDecompressed)
                throw GrfError("size mismatch");
            ok++;
        } catch (const std::exception& ex)
        {
            fail++;
            if (failures.size() < 8)
                failures.push_back(cp1252ToUtf8(e.relativePath) + ": " +
                                   ex.what());
        }
    }
    std::ostringstream ss;
    ss << "Verification: " << ok << " ok, " << fail << " failed";
    log(ss.str());
    for (const auto& f : failures)
        log("  ! " + f);
}

// ---------------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------------

void ArchivePanel::renderDialogs()
{
    if (openDialogOpen_)
    {
        ImGui::OpenPopup("Open archive");
        openDialogOpen_ = false;
    }
    if (saveDialogOpen_)
    {
        ImGui::OpenPopup("Save archive");
        saveDialogOpen_ = false;
    }
    if (addDialogOpen_)
    {
        ImGui::OpenPopup("Add file");
        addDialogOpen_ = false;
    }

    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal(
            "Open archive", nullptr, ImGuiWindowFlags_NoDocking))
    {
        ImGui::TextWrapped("Archive path (.grf / .gpf / .thor / .rgz):");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##openpath", openPathBuf_, sizeof openPathBuf_);
        if (ImGui::Button("Browse..."))
        {
            std::string picked;
            if (nativeFileDialog(false, picked))
                std::snprintf(
                    openPathBuf_, sizeof openPathBuf_, "%s", picked.c_str());
        }
        ImGui::SameLine();
        bool openClicked = ImGui::Button("Open");
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        if (openClicked && openPathBuf_[0] != '\0')
        {
            ImGui::CloseCurrentPopup();
            openArchive(openPathBuf_);
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal(
            "Save archive", nullptr, ImGuiWindowFlags_NoDocking))
    {
        ImGui::TextWrapped("Target path (leave as-is to repack in place):");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##savepath", savePathBuf_, sizeof savePathBuf_);
        if (ImGui::Button("Browse..."))
        {
            std::string picked;
            if (nativeFileDialog(true, picked))
                std::snprintf(
                    savePathBuf_, sizeof savePathBuf_, "%s", picked.c_str());
        }
        ImGui::SameLine();
        bool saveClicked = ImGui::Button("Save");
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        if (saveClicked && savePathBuf_[0] != '\0')
        {
            ImGui::CloseCurrentPopup();
            try
            {
                archive_->save(savePathBuf_);
                rebuildTree();
                log("Saved: " + std::string(savePathBuf_));
            } catch (const std::exception& ex)
            {
                log("Save failed: " + std::string(ex.what()));
            }
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Add file", nullptr, ImGuiWindowFlags_NoDocking))
    {
        ImGui::TextWrapped("Source file on disk:");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##sourcepath", addSourcePath_, sizeof addSourcePath_);
        if (ImGui::Button("Choose file..."))
        {
            std::string picked;
            if (nativeFileDialog(false, picked))
            {
                std::snprintf(addSourcePath_,
                              sizeof addSourcePath_,
                              "%s",
                              picked.c_str());
                std::string base = fs::path(picked).filename().string();
                if (base.empty())
                    base = "file.bin";
                const grf::FileEntry* e = selectedEntry();
                std::string dir;
                if (e)
                {
                    auto pathParts = splitBackslash(e->relativePath);
                    pathParts.pop_back();
                    for (const auto& p : pathParts)
                        dir += p + "\\";
                }
                std::snprintf(addTargetBuf_,
                              sizeof addTargetBuf_,
                              "%s%s",
                              dir.c_str(),
                              base.c_str());
            }
        }
        ImGui::Separator();
        ImGui::TextWrapped("Path inside the archive:");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##targetpath", addTargetBuf_, sizeof addTargetBuf_);
        bool addClicked = ImGui::Button("Add");
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        if (addClicked && addSourcePath_[0] != '\0' && addTargetBuf_[0] != '\0')
        {
            ImGui::CloseCurrentPopup();
            try
            {
                archive_->addFileFromDisk(addTargetBuf_, addSourcePath_, true);
                rebuildTree();
                log("Added " + std::string(addSourcePath_) + " -> " +
                    std::string(addTargetBuf_));
            } catch (const std::exception& ex)
            {
                log("Add failed: " + std::string(ex.what()));
            }
        }
        ImGui::EndPopup();
    }
}

} // namespace grfeditor