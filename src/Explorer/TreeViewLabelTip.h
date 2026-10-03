#pragma once

#include "InPlaceLabelTip.h"
#include <functional>

class TreeViewLabelTip : public InPlaceLabelTip {
public:
    using FontCallback = std::function<HFONT(HTREEITEM hItem)>;

    explicit TreeViewLabelTip(HWND hTreeView);
    ~TreeViewLabelTip() override = default;

    void SetFontCallback(FontCallback cb) { _fontCallback = std::move(cb); }

    // Window procedure message handlers called from TreeView subclass
    void OnMouseMove(WPARAM wParam, LPARAM lParam) override;
    void OnMouseHover() override;
    void OnMouseLeave() override;
    void OnScroll() override;
    void Hide() override;
    void Reset() override;

private:
    void ShowTip(HTREEITEM hItem);
    bool IsTextTruncated(HTREEITEM hItem, std::wstring& outText, RECT& outLabelRect) const;
    HFONT GetItemFont(HTREEITEM hItem) const;

    HTREEITEM _hoverItem = nullptr;
    HTREEITEM _displayedItem = nullptr;

    FontCallback _fontCallback;
};
