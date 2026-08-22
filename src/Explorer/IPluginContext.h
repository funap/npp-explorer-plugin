#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <filesystem>
#include <cstdint>
#include <windows.h>

#include "ThemeRenderer.h"

class IPluginContext {
public:
    virtual ~IPluginContext() = default;

    virtual HWND GetWindow() const = 0;
    virtual bool DoOpen(const std::filesystem::path& path) = 0;
    virtual std::wstring GetSelectedText() = 0;
    virtual COLORREF GetEditorDefaultForegroundColor() = 0;
    virtual COLORREF GetEditorDefaultBackgroundColor() = 0;
    virtual COLORREF GetEditorCurrentLineBackgroundColor() = 0;
    virtual bool IsDarkMode() = 0;
    virtual ThemeColors GetColors() = 0;
    virtual void SetFocusToCurrentEdit() = 0;
    virtual std::vector<std::wstring> GetSessionFiles(const std::filesystem::path& sessionFilePath) = 0;
    virtual std::filesystem::path GetCurrentDirectory() = 0;
    virtual int GetVersion() = 0;
    virtual bool IsSupportFluentUI() = 0;

    // New methods to abstract SendMessage calls in Explorer.cpp
    virtual void SetMenuItemCheck(int cmdID, bool visible) = 0;
    virtual void AddToolbarIcon(int cmdID, void* iconInfo, bool useDarkMode) = 0;
    virtual std::filesystem::path GetConfigDir() = 0;
    virtual HWND GetCurrentScintilla() = 0;
    virtual HWND GetMainScintilla() = 0;
    virtual HWND GetSecondScintilla() = 0;
    virtual std::filesystem::path GetFullCurrentPath() = 0;
    virtual int GetNbOpenFiles() = 0;
    virtual bool GetOpenFileNames(std::vector<std::wstring>& fileNames) = 0;

    // New semantic methods to further minimize GetWindow() usage
    virtual void LaunchFindFileDialog(const std::filesystem::path& directory) = 0;
    virtual void RunMenuCommand(int cmdID) = 0;
    virtual intptr_t SendMsgToPlugin(const std::wstring& destinationPluginName, void* communicationInfo) = 0;
};
