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


#include "PropDlg.h"

#include <algorithm>
#include <shlobj.h>

#include "../NppPlugin/Notepad_plus_msgs.h"

namespace {
constexpr WCHAR STR_DETAILS_HIDE[] = L"Details <<";
constexpr WCHAR STR_DETAILS_SHOW[] = L"Details >>";
}

// Set a call back with the handle after init to set the path.
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/reference/callbackfunctions/browsecallbackproc.asp
static int __stdcall BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM /*unused*/, LPARAM pData)
{
    if (uMsg == BFFM_INITIALIZED) {
        ::SendMessage(hwnd, BFFM_SETSELECTION, TRUE, pData);
    }
    return 0;
};


PropDlg::PropDlg()
    : StaticDialog()
    , _pName(nullptr)
    , _pLink(nullptr)
    , _pDesc(nullptr)
    , _linkDlg(LinkDlg::NONE)
    , _fileMustExist(FALSE)
    , _bWithLink(FALSE)
    , _seeDetails(FALSE)
    , _root(nullptr)
    , _iUImgPos(0)
    , _selectedGroup(nullptr)
{
}

PropDlg::~PropDlg()
{
}

INT_PTR PropDlg::doDialog(LPTSTR pName, LPTSTR pLink, LPTSTR pDesc, LinkDlg linkDlg, BOOL fileMustExist)
{
    _pName          = pName;
    _pLink          = pLink;
    _pDesc          = pDesc;
    _linkDlg        = linkDlg;
    _fileMustExist  = fileMustExist;
    return ::DialogBoxParam(_hInst, MAKEINTRESOURCE(IDD_PROP_DLG), _hParent,  (DLGPROC)dlgProc, (LPARAM)this);
}


INT_PTR CALLBACK PropDlg::run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message) {
        case WM_INITDIALOG: {
            /* set discription */
            WCHAR szBuffer[256];

            _stprintf(szBuffer, L"%ls:", _pDesc);
            ::SetWindowText(::GetDlgItem(_hSelf, IDC_STATIC_FAVES_DESC), szBuffer);

            /* if name is not defined extract from link */
            if (_pName && _pLink) {
                wcscpy(szBuffer, _pLink);
                if ((_pName[0] == '\0') && (szBuffer[0] != '\0')) {
                    if (szBuffer[wcslen(szBuffer) - 1] == '\\') {
                        szBuffer[wcslen(szBuffer) - 1] = '\0';
                    }
                    if (szBuffer[wcslen(szBuffer) - 1] == ':') {
                        wcscpy(_pName, szBuffer);
                    }
                    else {
                        wcscpy(_pName, (_tcsrchr(szBuffer, '\\') + 1));
                    }
                }
            }
            else {
                ::EnableWindow(::GetDlgItem(_hSelf, IDC_EDIT_NAME), FALSE);
                ::EnableWindow(::GetDlgItem(_hSelf, IDC_EDIT_LINK), FALSE);
                ::EnableWindow(::GetDlgItem(_hSelf, IDC_BTN_OPENDLG), FALSE);
            }

            /* set name and link */
            if (_pName){
                ::SetWindowText(::GetDlgItem(_hSelf, IDC_EDIT_NAME), _pName);
            }
            if (_pLink) {
                ::SetWindowText(::GetDlgItem(_hSelf, IDC_EDIT_LINK), _pLink);
            }

            SetFocus(::GetDlgItem(_hSelf, IDC_EDIT_NAME));
            _hTreeCtrl.Attach(::GetDlgItem(_hSelf, IDC_TREE_SELECT));

            goToCenter();

            if (_linkDlg == LinkDlg::NONE) {
                RECT rcName;
                RECT rcLink;

                ::ShowWindow(::GetDlgItem(_hSelf, IDC_BTN_OPENDLG), SW_HIDE);
                ::GetWindowRect(::GetDlgItem(_hSelf, IDC_EDIT_NAME), &rcName);
                ::GetWindowRect(::GetDlgItem(_hSelf, IDC_EDIT_LINK), &rcLink);

                rcLink.right = rcName.right;

                ScreenToClient(_hSelf, &rcLink);
                ::SetWindowPos(::GetDlgItem(_hSelf, IDC_EDIT_LINK), nullptr, rcLink.left, rcLink.top, rcLink.right-rcLink.left, rcLink.bottom-rcLink.top, SWP_NOZORDER);
            }

            if (_seeDetails == FALSE) {
                RECT rc = {0};

                ::DestroyWindow(::GetDlgItem(_hSelf, IDC_TREE_SELECT));
                ::DestroyWindow(::GetDlgItem(_hSelf, IDC_STATIC_SELECT));
                ::DestroyWindow(::GetDlgItem(_hSelf, IDC_BUTTON_DETAILS));

                /* resize window */
                ::GetWindowRect(_hSelf, &rc);
                rc.top      += 74;
                rc.bottom   -= 74;

                /* resize window and keep sure to resize correct */
                ::SetWindowPos(_hSelf, nullptr, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, SWP_NOZORDER);
            }
            else {
                /* get current icon offset */
                UINT iIconPos = _root->Type();

                /* set image list */
                ::SendMessage(_hTreeCtrl, TVM_SETIMAGELIST, TVSIL_NORMAL, (LPARAM)GetSmallImageList(FALSE));

                BOOL canExpand = std::any_of(_root->Children().cbegin(), _root->Children().cend(), [](const auto &elem) {
                    return elem->IsGroup();
                });

                HTREEITEM hItem = _hTreeCtrl.InsertItem(_root->Name(), iIconPos, _iUImgPos, 0, 0, TVI_ROOT, TVI_LAST, canExpand, _root);

                SendMessage(_hTreeCtrl, WM_SETREDRAW, FALSE, 0);
                ExpandTreeView(hItem);
                SendMessage(_hTreeCtrl, WM_SETREDRAW, TRUE, 0);

                if (_selectedGroup) {
                    hItem = _hTreeCtrl.FindTreeItemByParam(_selectedGroup);
                }
                _hTreeCtrl.SelectItem(hItem);

                ::SetWindowText(::GetDlgItem(_hSelf, IDC_BUTTON_DETAILS), L"Details <<");
            }
            break;
        }
        case WM_COMMAND : {
            switch (LOWORD(wParam)) {
            case IDC_BUTTON_DETAILS: {
                RECT    rc    = {0};

                /* toggle visibility state */
                _seeDetails ^= TRUE;

                /* resize window */
                ::GetWindowRect(_hSelf, &rc);

                if (_seeDetails == FALSE) {
                    ::ShowWindow(::GetDlgItem(_hSelf, IDC_TREE_SELECT), SW_HIDE);
                    ::ShowWindow(::GetDlgItem(_hSelf, IDC_STATIC_SELECT), SW_HIDE);

                    ::SetWindowText(::GetDlgItem(_hSelf, IDC_BUTTON_DETAILS), STR_DETAILS_SHOW);

                    rc.bottom -= 148;
                }
                else {
                    ::ShowWindow(::GetDlgItem(_hSelf, IDC_TREE_SELECT), SW_SHOW);
                    ::ShowWindow(::GetDlgItem(_hSelf, IDC_STATIC_SELECT), SW_SHOW);

                    ::SetWindowText(::GetDlgItem(_hSelf, IDC_BUTTON_DETAILS), STR_DETAILS_HIDE);

                    rc.bottom += 148;
                }

                ::SetWindowPos(_hSelf, nullptr, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, SWP_NOZORDER);
                break;
            }
            case IDC_BTN_OPENDLG: {
                if (_linkDlg == LinkDlg::FOLDER) {
                    // This code was copied and slightly modifed from:
                    // http://www.bcbdev.com/faqs/faq62.htm

                    // SHBrowseForFolder returns a PIDL. The memory for the PIDL is
                    // allocated by the shell. Eventually, we will need to free this
                    // memory, so we need to get a pointer to the shell malloc COM
                    // object that will free the PIDL later on.
                    LPMALLOC pShellMalloc = nullptr;
                    if (::SHGetMalloc(&pShellMalloc) == NO_ERROR) {
                        // If we were able to get the shell malloc object,
                        // then proceed by initializing the BROWSEINFO stuct
                        BROWSEINFO info;
                        ZeroMemory(&info, sizeof(info));
                        info.hwndOwner      = _hParent;
                        info.pidlRoot       = nullptr;
                        info.pszDisplayName = (LPTSTR)new WCHAR[MAX_PATH];
                        info.lpszTitle      = L"Select a folder:";
                        info.ulFlags        = BIF_RETURNONLYFSDIRS;
                        info.lpfn           = BrowseCallbackProc;
                        info.lParam         = (LPARAM)_pLink;

                        // Execute the browsing dialog.
                        PIDLIST_ABSOLUTE pidl = ::SHBrowseForFolder(&info);

                        // pidl will be nullptr if they cancel the browse dialog.
                        // pidl will be not nullptr when they select a folder.
                        if (pidl) {
                            // Try to convert the pidl to a display string.
                            // Return is true if success.
                            if (::SHGetPathFromIDList(pidl, _pLink)) {
                                // Set edit control to the directory path.
                                ::SetWindowText(::GetDlgItem(_hSelf, IDC_EDIT_LINK), _pLink);
                            }
                            pShellMalloc->Free(pidl);
                        }
                        pShellMalloc->Release();
                        delete [] info.pszDisplayName;
                    }
                }
                else {
                    LPTSTR  pszLink = nullptr;
                    FileDlg dlg(_hInst, _hParent);

                    dlg.setDefFileName(_pLink);
                    if (_tcsstr(_pDesc, L"Session") != nullptr) {
                        dlg.setExtFilter(L"Session file", L".session", nullptr);
                    }
                    dlg.setExtFilter(L"All types", L".*", nullptr);

                    if (_fileMustExist == TRUE) {
                        pszLink = dlg.doSaveDlg();
                    }
                    else {
                        pszLink = dlg.doOpenSingleFileDlg();
                    }

                    if (pszLink != nullptr) {
                        // Set edit control to the directory path.
                        wcscpy(_pLink, pszLink);
                        ::SetWindowText(::GetDlgItem(_hSelf, IDC_EDIT_LINK), _pLink);
                    }
                }
                break;
            }
            case IDCANCEL: {
                ::EndDialog(_hSelf, FALSE);
                return TRUE;
            }
            case IDOK: {
                if (_pName && _pLink) {
                    UINT lengthName = (UINT)::SendDlgItemMessage(_hSelf, IDC_EDIT_NAME, WM_GETTEXTLENGTH, 0, 0) + 1;
                    UINT lengthLink = (UINT)::SendDlgItemMessage(_hSelf, IDC_EDIT_LINK, WM_GETTEXTLENGTH, 0, 0) + 1;

                    SendDlgItemMessage(_hSelf, IDC_EDIT_NAME, WM_GETTEXT, lengthName, (LPARAM)_pName);
                    SendDlgItemMessage(_hSelf, IDC_EDIT_LINK, WM_GETTEXT, lengthLink, (LPARAM)_pLink);

                    if ((wcslen(_pName) != 0) && (wcslen(_pLink) != 0)) {
                        auto *selectedItem = _hTreeCtrl.GetSelection();
                        _selectedGroup = (FavesItemPtr)_hTreeCtrl.GetParam(selectedItem);
                        ::EndDialog(_hSelf, TRUE);
                        return TRUE;
                    }
                    ::MessageBox(_hParent, L"Fill out all fields!", L"Error", MB_OK);
                }
                else {
                    auto *selectedItem = _hTreeCtrl.GetSelection();
                    _selectedGroup = (FavesItemPtr)_hTreeCtrl.GetParam(selectedItem);
                    ::EndDialog(_hSelf, TRUE);
                    return TRUE;
                }
                break;
            }
            default:
                break;
            }
            break;
        }
        case WM_SIZE: {
            RECT    rc        = {0};
            RECT    rcMain    = {0};

            /* get main window size */
            ::GetWindowRect(_hSelf, &rcMain);

            /* resize static box */
            ::GetWindowRect(::GetDlgItem(_hSelf, IDC_STATIC_FAVES_DESC), &rc);
            rc.bottom   = rcMain.bottom - 46;
            ScreenToClient(_hSelf, &rc);
            ::SetWindowPos(::GetDlgItem(_hSelf, IDC_STATIC_FAVES_DESC), nullptr, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, SWP_NOZORDER);

            /* set position of OK button */
            ::GetWindowRect(::GetDlgItem(_hSelf, IDOK), &rc);
            rc.top      = rcMain.bottom - 36;
            rc.bottom   = rcMain.bottom - 12;
            ScreenToClient(_hSelf, &rc);
            ::SetWindowPos(::GetDlgItem(_hSelf, IDOK), nullptr, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, SWP_NOZORDER);

            /* set position of CANCEL button */
            ::GetWindowRect(::GetDlgItem(_hSelf, IDCANCEL), &rc);
            rc.top      = rcMain.bottom - 36;
            rc.bottom   = rcMain.bottom - 12;
            ScreenToClient(_hSelf, &rc);
            ::SetWindowPos(::GetDlgItem(_hSelf, IDCANCEL), nullptr, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, SWP_NOZORDER);

            /* set position of DETAILS button */
            ::GetWindowRect(::GetDlgItem(_hSelf, IDC_BUTTON_DETAILS), &rc);
            rc.top      = rcMain.bottom - 36;
            rc.bottom   = rcMain.bottom - 12;
            ScreenToClient(_hSelf, &rc);
            ::SetWindowPos(::GetDlgItem(_hSelf, IDC_BUTTON_DETAILS), nullptr, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, SWP_NOZORDER);
            break;
        }
        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            if (nmhdr->hwndFrom == _hTreeCtrl) {
                switch (nmhdr->code) {
                case TVN_SELCHANGED: {
                    /* only when link params are also viewed */
                    if (_bWithLink == TRUE) {
                        HTREEITEM hItem = _hTreeCtrl.GetSelection();

                        if (hItem != nullptr) {
                            FavesItemPtr pElem = (FavesItemPtr)_hTreeCtrl.GetParam(hItem);

                            if (pElem != nullptr) {
                                if (pElem->IsLink()) {
                                    ::SetDlgItemText(_hSelf, IDC_EDIT_NAME, pElem->Name().data());
                                    ::SetDlgItemText(_hSelf, IDC_EDIT_LINK, pElem->Link().data());
                                }
                                else {
                                    ::SetDlgItemText(_hSelf, IDC_EDIT_NAME, L"");
                                    ::SetDlgItemText(_hSelf, IDC_EDIT_LINK, L"");
                                }
                            }
                        }
                    }
                    break;
                }
                default:
                    break;
                }
            }
            break;
        }
        case WM_DESTROY: {
            /* deregister this dialog */
            ::SendMessage(_hParent, NPPM_MODELESSDIALOG, MODELESSDIALOGREMOVE, (LPARAM)_hSelf);
            break;
        }
        default:
            break;
    }
    return FALSE;
}

void PropDlg::setRoot(FavesItemPtr pElem, INT iUserImagePos, BOOL bWithLink)
{
    _root       = pElem;
    _iUImgPos   = iUserImagePos;
    _bWithLink  = bWithLink;

    /* do not destroy items */
    _seeDetails = TRUE;
}

FavesItemPtr PropDlg::getSelectedGroup() const
{
    return _selectedGroup;
}

void PropDlg::setSelectedGroup(FavesItemPtr group)
{
    _selectedGroup = group;
}

void PropDlg::ExpandTreeView(HTREEITEM hParentItem)
{
    FavesItemPtr parent = (FavesItemPtr)_hTreeCtrl.GetParam(hParentItem);
    if (parent == nullptr) {
        return;
    }

    for (auto&& child : parent->Children()) {
        BOOL haveChildren = FALSE;
        if (child->IsGroup()) {
            if (!child->Children().empty()) {
                if (child->Children().front()->IsGroup() || (_bWithLink == TRUE)) {
                    haveChildren = TRUE;
                }
            }
            // add new item
            HTREEITEM pCurrentItem = _hTreeCtrl.InsertItem(child->Name(), ICON_GROUP, ICON_GROUP, 0, 0, hParentItem, TVI_LAST, haveChildren, child.get());
            ExpandTreeView(pCurrentItem);
        }

        if (child->IsLink() && (_bWithLink == TRUE)) {
            // add new item
            INT iIconNormal     = 0;
            INT iIconSelected   = 0;
            INT iIconOverlayed  = 0;
            ExtractIcons(child->Name().c_str(), nullptr, DEVT_FILE, &iIconNormal, &iIconSelected, &iIconOverlayed);
            HTREEITEM pCurrentItem = _hTreeCtrl.InsertItem(child->Name(), _iUImgPos, _iUImgPos, 0, 0, hParentItem, TVI_LAST, haveChildren, child.get());
            ExpandTreeView(pCurrentItem);
        }
    }
    _hTreeCtrl.Expand(hParentItem, TVE_EXPAND);
}
