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


#include "TreeHelperClass.h"

#include <memory>


HTREEITEM TreeHelper::InsertItem(const std::wstring &itemName,
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

    return TreeView_InsertItem(_hTreeCtrl, &tvis);
}

void TreeHelper::DeleteChildren(HTREEITEM parentItem)
{
    HTREEITEM pCurrentItem = TreeView_GetNextItem(_hTreeCtrl, parentItem, TVGN_CHILD);

    while (pCurrentItem != nullptr) {
        TreeView_DeleteItem(_hTreeCtrl, pCurrentItem);
        pCurrentItem = TreeView_GetNextItem(_hTreeCtrl, parentItem, TVGN_CHILD);
    }
}

BOOL TreeHelper::UpdateItem(HTREEITEM hItem, 
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
    if ((haveChildren == FALSE) && delChildren && TreeView_GetChild(_hTreeCtrl, hItem)) {
        DeleteChildren(hItem);
    }

    return TreeView_SetItem(_hTreeCtrl, &item);
}

BOOL TreeHelper::GetItemText(HTREEITEM hItem, LPTSTR szBuf, INT bufSize)
{
    TVITEM tvi {
        .mask       = TVIF_TEXT,
        .hItem      = hItem,
        .pszText    = szBuf,
        .cchTextMax = bufSize,
    };
    BOOL bRet = TreeView_GetItem(_hTreeCtrl, &tvi);

    return bRet;
}

std::wstring TreeHelper::GetItemText(HTREEITEM hItem) const
{
    auto buffer = std::make_unique<wchar_t[]>(MAX_PATH);
    TVITEM tvi {
        .mask       = TVIF_TEXT,
        .hItem      = hItem,
        .pszText    = buffer.get(),
        .cchTextMax = MAX_PATH,
    };
    BOOL bRet = TreeView_GetItem(_hTreeCtrl, &tvi);
    if (!bRet) {
        return {};
    }
    return { buffer.get() };
}

void* TreeHelper::GetParam(HTREEITEM hItem)
{
    TVITEM tvi {
        .mask    = TVIF_PARAM,
        .hItem   = hItem,
        .lParam  = 0,
    };

    TreeView_GetItem(_hTreeCtrl, &tvi);

    return reinterpret_cast<void*>(tvi.lParam);
}

void TreeHelper::SetParam(HTREEITEM hItem, void* lParam)
{
    TVITEM item {
        .mask   = TVIF_PARAM,
        .hItem  = hItem,
        .lParam = reinterpret_cast<LPARAM>(lParam),
    };

    TreeView_SetItem(_hTreeCtrl, &item);
}


BOOL TreeHelper::GetItemIcons(HTREEITEM hItem, LPINT piIcon, LPINT piSelected, LPINT piOverlay)
{
    if ((piIcon == nullptr) || (piSelected == nullptr) || (piOverlay == nullptr)) {
        return FALSE;
    }

    TVITEM tvi;
    tvi.mask        = TVIF_STATE | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvi.hItem       = hItem;
    tvi.stateMask   = TVIS_OVERLAYMASK;

    BOOL bRet = TreeView_GetItem(_hTreeCtrl, &tvi);

    if (bRet) {
        *piIcon         = tvi.iImage;
        *piSelected     = tvi.iSelectedImage;
        *piOverlay      = (tvi.state >> 8) & 0xFF;
    }

    return bRet;
}

void TreeHelper::SetItemIcons(HTREEITEM hItem, INT icon, INT selected, INT overlay)
{
    TVITEM item;

    ZeroMemory(&item, sizeof(TVITEM));
    item.hItem = hItem;
    item.mask = TVIF_STATE | TVIF_IMAGE | TVIF_SELECTEDIMAGE;;
    item.iImage = icon;
    item.iSelectedImage = selected;
    item.state = overlay << 8;

    TreeView_SetItem(_hTreeCtrl, &item);
}

BOOL TreeHelper::IsItemExpanded(HTREEITEM hItem)
{
    return (BOOL)(TreeView_GetItemState(_hTreeCtrl, hItem, TVIS_EXPANDED) & TVIS_EXPANDED);
}

INT TreeHelper::GetChildrenCount(HTREEITEM item)
{
    INT count = 0;

    TVITEM tvChildItem;
    tvChildItem.mask = TVIF_PARAM;
    HTREEITEM childItem = TreeView_GetChild(_hTreeCtrl, item);
    while (childItem != nullptr) {
        tvChildItem.hItem = childItem;
        if (TreeView_GetItem(_hTreeCtrl, &tvChildItem)) {
            ++count;
        }
        childItem = TreeView_GetNextSibling(_hTreeCtrl, childItem);
    }
    return count;
}

std::vector<std::wstring> TreeHelper::GetItemPathFromRoot(HTREEITEM currentItem) const
{
    std::vector<std::wstring> result;

    if (currentItem != TVI_ROOT) {
        while (currentItem != nullptr) {
            result.emplace_back(GetItemText(currentItem));
            currentItem = TreeView_GetNextItem(_hTreeCtrl, currentItem, TVGN_PARENT);
        }
    }

    std::reverse(std::begin(result), std::end(result));
    return result;
}

HTREEITEM TreeHelper::FindTreeItemByParam(const void* param)
{
    auto FindTreeItemByParamRecursive = [&](const auto& self, HTREEITEM item) -> HTREEITEM {
        while (item != nullptr) {
            if (param == GetParam(item)) {
                return item;
            }

            HTREEITEM hChildItem = TreeView_GetChild(_hTreeCtrl, item);
            HTREEITEM hFoundItem = self(self, hChildItem);
            if (hFoundItem != nullptr) {
                return hFoundItem;
            }

            item = TreeView_GetNextSibling(_hTreeCtrl, item);
        }
        return nullptr;
    };

    HTREEITEM root = TreeView_GetRoot(_hTreeCtrl);
    return FindTreeItemByParamRecursive(FindTreeItemByParamRecursive, root);
}