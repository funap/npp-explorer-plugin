#pragma once

#include "InPlaceLabelTip.h"
#include <functional>

class ListViewLabelTip : public InPlaceLabelTip {
public:
    using FontCallback = std::function<HFONT(int itemIndex, int subItemIndex)>;

    explicit ListViewLabelTip(HWND hListView);
    ~ListViewLabelTip() override = default;

    void SetFontCallback(FontCallback cb) { _fontCallback = std::move(cb); }

    // Window procedure message handlers called from ListView subclass
    void OnMouseMove(WPARAM wParam, LPARAM lParam) override;
    void OnMouseHover() override;
    void OnMouseLeave() override;
    void OnScroll() override;
    void Hide() override;
    void Reset() override;

private:
    void ShowTip(int itemIndex, int subItemIndex);
    bool IsTextTruncated(int itemIndex, int subItemIndex, std::wstring& outText, RECT& outLabelRect) const;
    HFONT GetItemFont(int itemIndex, int subItemIndex) const;

    int _hoverItem = -1;
    int _hoverSubItem = -1;
    int _displayedItem = -1;
    int _displayedSubItem = -1;

    FontCallback _fontCallback;
};
