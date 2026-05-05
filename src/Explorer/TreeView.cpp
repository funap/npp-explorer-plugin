/*
This file is part of Explorer Plugin for Notepad++
Copyright (C)2006 Jens Lorenz <jens.plugin.npp@gmx.de>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include "TreeView.h"

#include <memory>
#include <algorithm>

BOOL TreeView::Attach(HWND wnd)
{
    _wnd = wnd;
    return TRUE;
}

HTREEITEM TreeView::InsertItem(const std::wstring &itemName,
                                 INT nImage,
                                 INT nSelectedImage,
                                 INT nOverlayedImage,
                                 BOOL bHidden,
                                 HTREEITEM hParent,
                                 HTREEITEM hInsertAfter,
                                 BOOL haveChildren,
                                 void* lParam)
{
    auto szItemName = std::make_unique<WCHAR[]>(MAX_PATH);
    itemName.copy(szItemName.get(), MAX_PATH);

    TV_INSERTSTRUCT tvis;
    ZeroMemory(&tvis, sizeof(TV_INSERTSTRUCT));
    tvis.hParent                = hParent;
    tvis.hInsertAfter           = hInsertAfter;
    tvis.item.mask              = TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_CHILDREN;
    tvis.item.pszText           = szItemName.get();
    tvis.item.iImage            = nImage;
    tvis.item.iSelectedImage    = nSelectedImage;
    tvis.item.cChildren         = haveChildren;
    tvis.item.lParam            = reinterpret_cast<LPARAM>(lParam);

    if (nOverlayedImage != 0) {
        tvis.item.mask      |= TVIF_STATE;
        tvis.item.state     |= INDEXTOOVERLAYMASK(nOverlayedImage);
        tvis.item.stateMask |= TVIS_OVERLAYMASK;
    }

    if (bHidden == TRUE) {
        tvis.item.mask      |= LVIF_STATE;
        tvis.item.state     |= LVIS_CUT;
        tvis.item.stateMask |= LVIS_CUT;
    }

    return TreeView_InsertItem(_wnd, &tvis);
}

void TreeView::DeleteChildren(HTREEITEM parentItem)
{
    auto pCurrentItem = TreeView_GetNextItem(_wnd, parentItem, TVGN_CHILD);

    while (pCurrentItem != nullptr) {
        TreeView_DeleteItem(_wnd, pCurrentItem);
        pCurrentItem = TreeView_GetNextItem(_wnd, parentItem, TVGN_CHILD);
    }
}

BOOL TreeView::UpdateItem(HTREEITEM hItem, 
                            const std::wstring &itemName, 
                            INT nImage, 
                            INT nSelectedImage, 
                            INT nOverlayedImage, 
                            BOOL bHidden,
                            BOOL haveChildren,
                            void* lParam,
                            BOOL delChildren)
{
    auto szItemName = std::make_unique<WCHAR[]>(MAX_PATH);
    itemName.copy(szItemName.get(), MAX_PATH);

    TVITEM item{
        .mask           = TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_CHILDREN | TVIF_STATE,
        .hItem          = hItem,
        .state          = static_cast<UINT>(INDEXTOOVERLAYMASK(nOverlayedImage)),
        .stateMask      = TVIS_OVERLAYMASK,
        .pszText        = szItemName.get(),
        .iImage         = nImage,
        .iSelectedImage = nSelectedImage,
        .cChildren      = haveChildren,
        .lParam         = reinterpret_cast<LPARAM>(lParam),
    };

    /* mark as cut if the icon is hidden */
    if (bHidden == TRUE) {
        item.state      |= LVIS_CUT;
        item.stateMask  |= LVIS_CUT;
    }

    /* delete children items when available but not needed */
    if ((haveChildren == FALSE) && delChildren && TreeView_GetChild(_wnd, hItem)) {
        DeleteChildren(hItem);
    }

    return TreeView_SetItem(_wnd, &item);
}

BOOL TreeView::GetItemText(HTREEITEM hItem, LPTSTR szBuf, INT bufSize) const
{
    TVITEM tvi {
        .mask       = TVIF_TEXT,
        .hItem      = hItem,
        .pszText    = szBuf,
        .cchTextMax = bufSize,
    };
    BOOL bRet = TreeView_GetItem(_wnd, &tvi);

    return bRet;
}

std::wstring TreeView::GetItemText(HTREEITEM hItem) const
{
    auto buffer = std::make_unique<wchar_t[]>(MAX_PATH);
    TVITEM tvi {
        .mask       = TVIF_TEXT,
        .hItem      = hItem,
        .pszText    = buffer.get(),
        .cchTextMax = MAX_PATH,
    };
    BOOL bRet = TreeView_GetItem(_wnd, &tvi);
    if (!bRet) {
        return {};
    }
    return { buffer.get() };
}

void* TreeView::GetParam(HTREEITEM hItem) const
{
    TVITEM tvi {
        .mask    = TVIF_PARAM,
        .hItem   = hItem,
        .lParam  = 0,
    };

    TreeView_GetItem(_wnd, &tvi);

    return reinterpret_cast<void*>(tvi.lParam);
}

void TreeView::SetParam(HTREEITEM hItem, void* lParam)
{
    TVITEM item {
        .mask   = TVIF_PARAM,
        .hItem  = hItem,
        .lParam = reinterpret_cast<LPARAM>(lParam),
    };

    TreeView_SetItem(_wnd, &item);
}

BOOL TreeView::SetItemHasChildren(HTREEITEM item, BOOL hasChildren) {
    TVITEM data{
        .mask           = TVIF_CHILDREN,
        .hItem          = item,
        .cChildren      = hasChildren,
    };
    return TreeView_SetItem(_wnd, &data);
}

BOOL TreeView::GetItemIcons(HTREEITEM hItem, LPINT piIcon, LPINT piSelected, LPINT piOverlay) const
{
    if ((piIcon == nullptr) || (piSelected == nullptr) || (piOverlay == nullptr)) {
        return FALSE;
    }

    TVITEM tvi;
    tvi.mask        = TVIF_STATE | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvi.hItem       = hItem;
    tvi.stateMask   = TVIS_OVERLAYMASK;

    BOOL bRet = TreeView_GetItem(_wnd, &tvi);

    if (bRet) {
        *piIcon         = tvi.iImage;
        *piSelected     = tvi.iSelectedImage;
        *piOverlay      = (tvi.state >> 8) & 0xFF;
    }

    return bRet;
}

void TreeView::SetItemIcons(HTREEITEM hItem, INT icon, INT selected, INT overlay)
{
    TVITEM item;

    ZeroMemory(&item, sizeof(TVITEM));
    item.hItem = hItem;
    item.mask = TVIF_STATE | TVIF_IMAGE | TVIF_SELECTEDIMAGE;;
    item.iImage = icon;
    item.iSelectedImage = selected;
    item.state = overlay << 8;

    TreeView_SetItem(_wnd, &item);
}

BOOL TreeView::IsItemExpanded(HTREEITEM hItem) const
{
    return (BOOL)(TreeView_GetItemState(_wnd, hItem, TVIS_EXPANDED) & TVIS_EXPANDED);
}

INT TreeView::GetChildrenCount(HTREEITEM item) const
{
    INT count = 0;

    TVITEM tvChildItem;
    tvChildItem.mask = TVIF_PARAM;
    HTREEITEM childItem = TreeView_GetChild(_wnd, item);
    while (childItem != nullptr) {
        tvChildItem.hItem = childItem;
        if (TreeView_GetItem(_wnd, &tvChildItem)) {
            ++count;
        }
        childItem = TreeView_GetNextSibling(_wnd, childItem);
    }
    return count;
}

std::vector<std::wstring> TreeView::GetItemPathFromRoot(HTREEITEM currentItem) const
{
    std::vector<std::wstring> result;

    if (currentItem != TVI_ROOT) {
        while (currentItem != nullptr) {
            result.emplace_back(GetItemText(currentItem));
            currentItem = TreeView_GetNextItem(_wnd, currentItem, TVGN_PARENT);
        }
    }

    std::reverse(std::begin(result), std::end(result));
    return result;
}

HTREEITEM TreeView::FindTreeItemByParam(const void* param)
{
    auto FindTreeItemByParamRecursive = [&](const auto& self, HTREEITEM item) -> HTREEITEM {
        while (item != nullptr) {
            if (param == GetParam(item)) {
                return item;
            }

            HTREEITEM hChildItem = TreeView_GetChild(_wnd, item);
            HTREEITEM hFoundItem = self(self, hChildItem);
            if (hFoundItem != nullptr) {
                return hFoundItem;
            }

            item = TreeView_GetNextSibling(_wnd, item);
        }
        return nullptr;
    };

    auto root = TreeView_GetRoot(_wnd);
    return FindTreeItemByParamRecursive(FindTreeItemByParamRecursive, root);
}

HIMAGELIST TreeView::SetImageList(HIMAGELIST images, int type /* =TVSIL_NORMAL */)
{
    return TreeView_SetImageList(_wnd, images, type);
}

HTREEITEM TreeView::HitTest(TVHITTESTINFO* hitInfo) const
{
    return TreeView_HitTest(_wnd, hitInfo);
}

HTREEITEM TreeView::GetSelection() const
{
    return TreeView_GetSelection(_wnd);
}

HTREEITEM TreeView::GetRoot() const
{
    return TreeView_GetRoot(_wnd);
}

HTREEITEM TreeView::GetChild(HTREEITEM item) const
{
    return TreeView_GetChild(_wnd, item);
}

HTREEITEM TreeView::GetParent(HTREEITEM item) const
{
    return TreeView_GetParent(_wnd, item);
}

HTREEITEM TreeView::GetNextItem(HTREEITEM item, UINT code) const
{
    return TreeView_GetNextItem(_wnd, item, code);
}

HTREEITEM TreeView::GetDropHilightItem() const
{
    return TreeView_GetDropHilight(_wnd);
}

HTREEITEM TreeView::GetLastVisibleItem() const
{
    return TreeView_GetLastVisible(_wnd);
}

BOOL TreeView::ItemHasChildren(HTREEITEM item) const
{
    return TreeView_GetChild(_wnd, item) ? TRUE : FALSE;
}

BOOL TreeView::Expand(HTREEITEM item, UINT code) const
{
    return TreeView_Expand(_wnd, item, code);
}

BOOL TreeView::GetItemRect(HTREEITEM item, RECT* rect, BOOL isTextOnly) const
{
    return TreeView_GetItemRect(_wnd, item, rect, isTextOnly);
}

BOOL TreeView::SelectItem(HTREEITEM item)
{
    return TreeView_SelectItem(_wnd, item);
}

BOOL TreeView::DeleteItem(HTREEITEM item)
{
    return TreeView_DeleteItem(_wnd, item);
}

BOOL TreeView::EnsureVisible(HTREEITEM item)
{
    return TreeView_EnsureVisible(_wnd, item);
}

BOOL TreeView::SelectDropTarget(HTREEITEM item)
{
    return TreeView_SelectDropTarget(_wnd, item);
}
