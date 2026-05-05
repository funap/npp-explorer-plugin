#pragma once

#include "IEditor.h"
#include "../NppPlugin/PluginInterface.h"

class Editor : public IEditor {
public:
    static Editor& Instance();

    void SetNppData(NppData nppData);

    // IEditor implementation
    HWND GetWindow() const override;
    bool DoOpen(const std::filesystem::path& path) override;
    std::wstring GetSelectedText() override;
    COLORREF GetEditorDefaultForegroundColor() override;
    COLORREF GetEditorDefaultBackgroundColor() override;
    COLORREF GetEditorCurrentLineBackgroundColor() override;
    bool IsDarkMode() override;
    EditorColors GetColors() override;
    void SetFocusToCurrentEdit() override;
    std::vector<std::wstring> GetSessionFiles(const std::filesystem::path& sessionFilePath) override;
    std::filesystem::path GetCurrentDirectory() override;
    int GetVersion() override;
    bool IsSupportFluentUI() override;

    void SetMenuItemCheck(int cmdID, bool visible) override;
    void AddToolbarIcon(int cmdID, void* iconInfo, bool useDarkMode) override;
    std::filesystem::path GetConfigDir() override;
    HWND GetCurrentScintilla() override;
    HWND GetMainScintilla() override;
    HWND GetSecondScintilla() override;
    std::filesystem::path GetFullCurrentPath() override;
    int GetNbOpenFiles() override;
    bool GetOpenFileNames(std::vector<std::wstring>& fileNames) override;

    void LaunchFindFileDialog(const std::filesystem::path& directory) override;
    void RunMenuCommand(int cmdID) override;
    intptr_t SendMsgToPlugin(const std::wstring& destinationPluginName, void* communicationInfo) override;

private:
    Editor() = default;
    NppData _nppData{};
};
