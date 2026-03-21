#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "FileFilter.h"

#include <filesystem>
#include <string>
#include <vector>
#include <memory>

enum class SizeFmt : int {
    SFMT_BYTES,
    SFMT_KBYTE,
    SFMT_DYNAMIC,
    SFMT_DYNAMIC_EX,
    SFMT_MAX,
};

enum class DateFmt : int {
    DFMT_ENG,
    DFMT_GER,
    DFMT_MAX,
};

struct NppExecScripts {
    std::wstring szScriptName;
    std::wstring szArguments;
};

struct NppExecProp {
    std::wstring szAppName;
    std::wstring szScriptPath;
    std::vector<NppExecScripts> vNppExecScripts;
};

struct CphProgram {
    std::wstring szAppName;
};

class Settings {
public:
    struct FontDeleter {
        using pointer = HFONT;
        void operator()(HFONT h) const { if (h) ::DeleteObject(h); }
    };
    using UniqueFont = std::unique_ptr<HFONT, FontDeleter>;

    void Load(const std::filesystem::path& configDir);
    void Save();
    void InitializeFonts();

    const std::filesystem::path& GetConfigDir() const { return _configPath; }
    const std::filesystem::path& GetIniFilePath() const { return _iniFilePath; }

    // Getters and Setters
    const std::wstring& GetCurrentDir() const { return _currentDir; }
    void SetCurrentDir(const std::wstring& dir) { _currentDir = dir; }

    const std::wstring& GetRootFolder() const { return _rootFolder; }
    void SetRootFolder(const std::wstring& folder) { _rootFolder = folder; }

    HFONT GetDefaultFont() const { return _defaultFont.get(); }
    HFONT GetUnderlineFont() const { return _underlineFont.get(); }
    const LOGFONT& GetLogFont() const { return _logFont; }
    void SetLogFont(const LOGFONT& lf) { _logFont = lf; }

    int GetSplitterPos() const { return _iSplitterPos; }
    void SetSplitterPos(int pos) { _iSplitterPos = pos; }

    int GetSplitterPosHorizontal() const { return _iSplitterPosHorizontal; }
    void SetSplitterPosHorizontal(int pos) { _iSplitterPosHorizontal = pos; }

    bool IsAscending() const { return _bAscending; }
    void SetAscending(bool asc) { _bAscending = asc; }

    int GetSortPos() const { return _iSortPos; }
    void SetSortPos(int pos) { _iSortPos = pos; }

    int GetColumnPosName() const { return _iColumnPosName; }
    void SetColumnPosName(int pos) { _iColumnPosName = pos; }

    int GetColumnPosExt() const { return _iColumnPosExt; }
    void SetColumnPosExt(int pos) { _iColumnPosExt = pos; }

    int GetColumnPosSize() const { return _iColumnPosSize; }
    void SetColumnPosSize(int pos) { _iColumnPosSize = pos; }

    int GetColumnPosDate() const { return _iColumnPosDate; }
    void SetColumnPosDate(int pos) { _iColumnPosDate = pos; }

    bool IsShowHidden() const { return _bShowHidden; }
    void SetShowHidden(bool show) { _bShowHidden = show; }

    bool IsViewBraces() const { return _bViewBraces; }
    void SetViewBraces(bool view) { _bViewBraces = view; }

    bool IsViewLong() const { return _bViewLong; }
    void SetViewLong(bool view) { _bViewLong = view; }

    bool IsAddExtToName() const { return _bAddExtToName; }
    void SetAddExtToName(bool add) { _bAddExtToName = add; }

    bool IsAutoUpdate() const { return _bAutoUpdate; }
    void SetAutoUpdate(bool autoUpd) { _bAutoUpdate = autoUpd; }

    bool IsAutoNavigate() const { return _bAutoNavigate; }
    void SetAutoNavigate(bool autoNav) { _bAutoNavigate = autoNav; }

    bool IsHideFoldersInFileList() const { return _bHideFoldersInFileList; }
    void SetHideFoldersInFileList(bool hide) { _bHideFoldersInFileList = hide; }

    SizeFmt GetFmtSize() const { return _fmtSize; }
    void SetFmtSize(SizeFmt fmt) { _fmtSize = fmt; }

    DateFmt GetFmtDate() const { return _fmtDate; }
    void SetFmtDate(DateFmt fmt) { _fmtDate = fmt; }

    std::vector<std::wstring>& GetFilterHistory() { return _vStrFilterHistory; }
    const std::vector<std::wstring>& GetFilterHistory() const { return _vStrFilterHistory; }
    void SetFilterHistory(const std::vector<std::wstring>& history) { _vStrFilterHistory = history; }

    FileFilter& GetFileFilter() { return _fileFilter; }
    const FileFilter& GetFileFilter() const { return _fileFilter; }

    UINT GetTimeout() const { return _uTimeout; }
    void SetTimeout(UINT timeout) { _uTimeout = timeout; }

    bool IsUseSystemIcons() const { return _bUseSystemIcons; }
    void SetUseSystemIcons(bool use) { _bUseSystemIcons = use; }

    NppExecProp& GetNppExecProp() { return _nppExecProp; }
    const NppExecProp& GetNppExecProp() const { return _nppExecProp; }

    CphProgram& GetCphProgram() { return _cphProgram; }
    const CphProgram& GetCphProgram() const { return _cphProgram; }

    size_t GetMaxHistorySize() const { return _maxHistorySize; }
    void SetMaxHistorySize(size_t size) { _maxHistorySize = size; }

    bool IsUseFullTree() const { return _bUseFullTree; }
    void SetUseFullTree(bool use) { _bUseFullTree = use; }

private:
    std::filesystem::path       _configPath;
    std::filesystem::path       _iniFilePath;

    std::wstring                _currentDir;
    std::wstring                _rootFolder;
    LOGFONT                     _logFont                {};
    UniqueFont                  _defaultFont;
    UniqueFont                  _underlineFont;
    int                         _iSplitterPos           = 120;
    int                         _iSplitterPosHorizontal = 200;
    bool                        _bAscending             = true;
    int                         _iSortPos               = 0;
    int                         _iColumnPosName         = 150;
    int                         _iColumnPosExt          = 50;
    int                         _iColumnPosSize         = 70;
    int                         _iColumnPosDate         = 100;
    bool                        _bShowHidden            = false;
    bool                        _bViewBraces            = true;
    bool                        _bViewLong              = false;
    bool                        _bAddExtToName          = false;
    bool                        _bAutoUpdate            = true;
    bool                        _bAutoNavigate          = false;
    bool                        _bHideFoldersInFileList = false;
    SizeFmt                     _fmtSize                = SizeFmt::SFMT_KBYTE;
    DateFmt                     _fmtDate                = DateFmt::DFMT_ENG;
    std::vector<std::wstring>   _vStrFilterHistory      {};
    FileFilter                  _fileFilter             {};
    UINT                        _uTimeout               = 1000;
    bool                        _bUseSystemIcons        = true;
    NppExecProp                 _nppExecProp            {};
    CphProgram                  _cphProgram             {};
    size_t                      _maxHistorySize         = 50;
    bool                        _bUseFullTree           = false;
};
