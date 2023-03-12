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

#include "FavesDialog.h"

#include <dbt.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <uxtheme.h>
#include <Vsstyle.h>
#include <sstream>
#include <functional>

#include "Explorer.h"
#include "ExplorerDialog.h"
#include "ExplorerResource.h"
#include "NewDlg.h"
#include "resource.h"
#include "NppInterface.h"
#include "StringUtil.h"


static ToolBarButtonUnit toolBarIcons[] = {
	{IDM_EX_EXPLORER,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_TB_EXPLORER, 0},

	//-------------------------------------------------------------------------------------//
	{0,						IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, 0},
	//-------------------------------------------------------------------------------------//

    {IDM_EX_LINK_NEW_FILE,  IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_LINKNEWFILE, 0},
    {IDM_EX_LINK_NEW_FOLDER,IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_LINKNEWFOLDER, 0},

	//-------------------------------------------------------------------------------------//
	{0,						IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, 0},
	//-------------------------------------------------------------------------------------//

    {IDM_EX_LINK_NEW,  		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_LINKNEW, 0},
	{IDM_EX_LINK_DELETE,	IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_LINKDELETE, 0},

	//-------------------------------------------------------------------------------------//
	{0,						IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, 0},
	//-------------------------------------------------------------------------------------//

    {IDM_EX_LINK_EDIT,	    IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_LINKEDIT, 0}
};    

static TCHAR FAVES_DATA[] = _T("\\Favorites.dat");

static LPCTSTR szToolTip[] = {
	_T("Explorer"),
	_T("Link Current File..."),
	_T("Link Current Folder..."),
	_T("New Link..."),
	_T("Delete Link"),
	_T("Edit Link...")
};

static LPCTSTR cFavesItemNames[] = {
	_T("[Folders]"),
	_T("[Files]"),
	_T("[Web]"),
	_T("[Sessions]"),
};

LPCTSTR FavesDialog::GetNameStrFromCmd(UINT resID)
{
	if ((IDM_EX_EXPLORER <= resID) && (resID <= IDM_EX_LINK_EDIT)) {
		return szToolTip[resID - IDM_EX_EXPLORER];
	}
	return nullptr;
}

FavesDialog::FavesDialog(void) : DockingDlgInterface(IDD_EXPLORER_DLG)
{
	_hTreeCtrl				= nullptr;
	_isCut					= FALSE;
	_hTreeCutCopy			= nullptr;
	_addToSession			= FALSE;
}

FavesDialog::~FavesDialog(void)
{
	ImageList_Destroy(_hImageList);
}


void FavesDialog::init(HINSTANCE hInst, HWND hParent, ExProp *prop)
{
	_pExProp = prop;
	DockingDlgInterface::init(hInst, hParent);

	/* init database */
	ReadSettings();
}


void FavesDialog::doDialog(bool willBeShown)
{
    if (!isCreated())
	{
        tTbData data{};
        create(&data);

        // define the default docking behaviour
        data.pszName = _T("Favorites");
        data.dlgID = DOCKABLE_FAVORTIES_INDEX;
        data.uMask = DWS_DF_CONT_LEFT | DWS_ICONTAB;
        data.hIconTab = (HICON)::LoadImage(_hInst, MAKEINTRESOURCE(IDI_HEART), IMAGE_ICON, 0, 0, LR_LOADMAP3DCOLORS | LR_LOADTRANSPARENT);
        data.pszModuleName = getPluginFileName();

        ::SendMessage(_hParent, NPPM_DMMREGASDCKDLG, 0, (LPARAM)&data);

        // NPP steals CustomDraw, so SubClassifies itself.
        SetWindowTheme(_hTreeCtrl, L"Explorer", nullptr);
        SetWindowSubclass(_hSelf, dlgProcSub, 'dlg', reinterpret_cast<DWORD_PTR>(this));

        /* Update "Add current..." icons */
		NotifyNewFile();
		ExpandElementsRecursive(TVI_ROOT);
	}

	UpdateColors();
    display(willBeShown);
}


void FavesDialog::SaveSession(void)
{
	AddSaveSession(nullptr, TRUE);
}


void FavesDialog::NotifyNewFile(void)
{
	if (isCreated() && isVisible())
	{
		TCHAR TEMP[MAX_PATH] = {};

		/* update "new file link" icon */
		::SendMessage(_hParent, NPPM_GETFULLCURRENTPATH, 0, (LPARAM)TEMP);
		_ToolBar.enable(IDM_EX_LINK_NEW_FILE, PathFileExists(TEMP));

		/* update "new folder link" icon */
		::SendMessage(_hParent, NPPM_GETCURRENTDIRECTORY, 0, (LPARAM)TEMP);
		_ToolBar.enable(IDM_EX_LINK_NEW_FOLDER, (_tcslen(TEMP) != 0));
	}
}


INT_PTR CALLBACK FavesDialog::run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message) {
	case WM_INITDIALOG:
		/* get handle of dialogs */
		_hTreeCtrl		= ::GetDlgItem(_hSelf, IDC_TREE_FOLDER);
		::DestroyWindow(::GetDlgItem(_hSelf, IDC_LIST_FILE));
		::DestroyWindow(::GetDlgItem(_hSelf, IDC_BUTTON_SPLITTER));
		::DestroyWindow(::GetDlgItem(_hSelf, IDC_STATIC_FILTER));
		::DestroyWindow(::GetDlgItem(_hSelf, IDC_COMBO_FILTER));

		InitialDialog();
		break;
	case WM_NOTIFY: {
		LPNMHDR		nmhdr = (LPNMHDR)lParam;

		if (nmhdr->hwndFrom == _hTreeCtrl) {
			switch (nmhdr->code) {
			case NM_RCLICK: {
				DWORD dwpos = ::GetMessagePos();
				POINT pt = {
					.x = GET_X_LPARAM(dwpos),
					.y = GET_Y_LPARAM(dwpos),
				};
				TVHITTESTINFO ht = {
					.pt = pt
				};
				::ScreenToClient(_hTreeCtrl, &ht.pt);
				HTREEITEM hItem = TreeView_HitTest(_hTreeCtrl, &ht);
				if (hItem != nullptr) {
					OpenContext(hItem, pt);
				}
				break;
			}
			case TVN_ITEMEXPANDING: {
				LPNMTREEVIEW pnmtv = reinterpret_cast<LPNMTREEVIEW>(lParam);
				HTREEITEM hItem = pnmtv->itemNew.hItem;

				if (hItem != nullptr) {
					/* get element information */
					PELEM pElem = reinterpret_cast<PELEM>(pnmtv->itemNew.lParam);

					/* update expand state */
					pElem->uParam ^= FAVES_PARAM_EXPAND;

					// reload session's children
					if (((pElem->uParam & FAVES_PARAM) == FAVES_SESSIONS) &&
						((pElem->uParam & FAVES_PARAM_LINK) == FAVES_PARAM_LINK)) {
						DeleteChildren(hItem);
						DrawSessionChildren(hItem);
					}

					if (!TreeView_GetChild(_hTreeCtrl, hItem)) {
						if (pElem == nullptr) {
							/* nothing to do */
						}
						else if (((pElem->uParam & FAVES_PARAM) == FAVES_SESSIONS) && 
									((pElem->uParam & FAVES_PARAM_LINK) == FAVES_PARAM_LINK)) {
							DeleteChildren(hItem);
							DrawSessionChildren(hItem);
						}
						else {
							UpdateLink(hItem);
						}
					}
				}
				break;
			}
			case TVN_SELCHANGED: {
				HTREEITEM hItem = TreeView_GetSelection(_hTreeCtrl);

				if (hItem != nullptr) {
					PELEM pElem = (PELEM)GetParam(hItem);

					if (pElem != nullptr) {
						_ToolBar.enable(IDM_EX_LINK_NEW, !(pElem->uParam & FAVES_PARAM_LINK));
						_ToolBar.enable(IDM_EX_LINK_EDIT, !(pElem->uParam & FAVES_PARAM_MAIN));
						_ToolBar.enable(IDM_EX_LINK_DELETE, !(pElem->uParam & FAVES_PARAM_MAIN));
						NotifyNewFile();
					}
					else {
						_ToolBar.enable(IDM_EX_LINK_NEW, false);
						_ToolBar.enable(IDM_EX_LINK_EDIT, false);
						_ToolBar.enable(IDM_EX_LINK_DELETE, false);
						NotifyNewFile();
					}
				}						
				break;
			}
			case TVN_GETINFOTIP: {
				LPNMTVGETINFOTIP pTip = reinterpret_cast<LPNMTVGETINFOTIP>(lParam);
				HTREEITEM item = pTip->hItem;

				PELEM pElem = reinterpret_cast<PELEM>(GetParam(item));
				if (pElem) {
					// show full file path
					std::wstring tipText;
					if (!pElem->link.empty()) {
						tipText += pElem->link;
					}

					const BOOL isLink = ((pElem->uParam & FAVES_PARAM_LINK) == FAVES_PARAM_LINK);
					const BOOL isSession = ((pElem->uParam & FAVES_PARAM) == FAVES_SESSIONS);
					if (isLink && isSession) {
						INT count = GetChildrenCount(item);
						if (count > 0) {
							// Check non-existent files
							auto sessionFiles = NppInterface::getSessionFiles(pElem->link);
							int nonExistentFileCount = 0;
							for (auto &&file : sessionFiles) {
								if (!::PathFileExists(file.c_str())) {
									++nonExistentFileCount;
								}
							}

							// make tooltip text.
							tipText += StringUtil::format(L"\nThis session has %d files", count);
							if (nonExistentFileCount > 0) {
								tipText += StringUtil::format(L" (%d are non-existent)", nonExistentFileCount);
							}
							tipText += L".";
						}
					}
					if (!tipText.empty()) {
						wcscpy_s(pTip->pszText, pTip->cchTextMax, tipText.c_str());
						return TRUE;
					}
					return FALSE; // show default tooltip text
				}
				break;
			}
			default:
				break;
			}
		}
		else if ((nmhdr->hwndFrom == _Rebar.getHSelf()) && (nmhdr->code == RBN_CHEVRONPUSHED)) {
			NMREBARCHEVRON * lpnm = (NMREBARCHEVRON*)nmhdr;
			if (lpnm->wID == REBAR_BAR_TOOLBAR) {
				POINT pt = {
					.x = lpnm->rc.left,
					.y = lpnm->rc.bottom,
				};
				ClientToScreen(nmhdr->hwndFrom, &pt);
				tb_cmd(_ToolBar.doPopop(pt));
				return TRUE;
			}
			break;
		}
		else if (nmhdr->code == TTN_GETDISPINFO) {
			LPTOOLTIPTEXT lpttt = reinterpret_cast<LPTOOLTIPTEXT>(nmhdr); 
			lpttt->hinst = _hInst; 

			// Specify the resource identifier of the descriptive 
			// text for the given button.
			int idButton = int(lpttt->hdr.idFrom);
			lpttt->lpszText = const_cast<LPTSTR>(GetNameStrFromCmd(idButton));
			return TRUE;
		}

		DockingDlgInterface::run_dlgProc(Message, wParam, lParam);

		return FALSE;
	}
	case WM_SIZE:
	case WM_MOVE: {
		RECT rc = {};
        /* set position of toolbar */
        getClientRect(rc);
        _ToolBar.reSizeTo(rc);
        _Rebar.reSizeTo(rc);

        auto toolBarHeight = _ToolBar.getHeight();

		/* set position of tree control */
		rc.top    += toolBarHeight;
		rc.bottom -= toolBarHeight;
		::SetWindowPos(_hTreeCtrl, nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

		break;
	}
	case WM_COMMAND:
		// ESC key has been pressed
		if (LOWORD(wParam) == IDCANCEL) {
			NppInterface::setFocusToCurrentEdit();
			return TRUE;
		}

		if ((HWND)lParam == _ToolBar.getHSelf()) {
			tb_cmd(LOWORD(wParam));
		}
		break;
	case WM_PAINT:
		::RedrawWindow(_ToolBar.getHSelf(), nullptr, nullptr, TRUE);
		break;
	case WM_DESTROY:
		SaveSettings();
		_vDB.clear();
		_ToolBar.destroy();
		break;
	case EXM_OPENLINK:
		OpenLink(_peOpenLink);
		break;
	default:
		DockingDlgInterface::run_dlgProc(Message, wParam, lParam);
		break;
	}

	return FALSE;
}

LRESULT FavesDialog::CustomDrawProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    if (Message == WM_NOTIFY) {
        LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>(lParam);
        if ((NM_CUSTOMDRAW == nmhdr->code) && (nmhdr->hwndFrom == _hTreeCtrl)) {
            LPNMTVCUSTOMDRAW cd = (LPNMTVCUSTOMDRAW)lParam;

            static HTHEME s_theme = nullptr;
            switch (cd->nmcd.dwDrawStage) {
            case CDDS_PREPAINT:
                s_theme = OpenThemeData(nmhdr->hwndFrom, L"TreeView");
                return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
            case CDDS_ITEMPREPAINT: {
                HTREEITEM   hItem = reinterpret_cast<HTREEITEM>(cd->nmcd.dwItemSpec);

                // background
                auto maskedItemState = cd->nmcd.uItemState & (CDIS_SELECTED | CDIS_HOT);
                int itemState   = maskedItemState == (CDIS_SELECTED | CDIS_HOT) ? TREIS_HOTSELECTED
                                : maskedItemState == CDIS_SELECTED              ? TREIS_SELECTED
                                : maskedItemState == CDIS_HOT                   ? TREIS_HOT
                                : TREIS_NORMAL;
                if ((itemState == TREIS_SELECTED) && (nmhdr->hwndFrom != GetFocus())) {
                    itemState = TREIS_SELECTEDNOTFOCUS;
                }
                if (itemState != TREIS_NORMAL) {
                    DrawThemeBackground(s_theme, cd->nmcd.hdc, TVP_TREEITEM, itemState, &cd->nmcd.rc, &cd->nmcd.rc);
                }

                // [+]/[-] signs
                RECT glyphRect{};
                TVGETITEMPARTRECTINFO info{
                    .hti = hItem,
                    .prc = &glyphRect,
                    .partID = TVGIPR_BUTTON
                };
                if (TRUE == SendMessage(nmhdr->hwndFrom, TVM_GETITEMPARTRECT, 0, (LPARAM)&info)) {
                    BOOL isExpand = (TreeView_GetItemState(nmhdr->hwndFrom, hItem, TVIS_EXPANDED) & TVIS_EXPANDED) ? TRUE : FALSE;
                    const int glyphStates = isExpand ? GLPS_OPENED : GLPS_CLOSED;

                    SIZE glythSize;
                    GetThemePartSize(s_theme, cd->nmcd.hdc, TVP_GLYPH, glyphStates, nullptr, THEMESIZE::TS_DRAW, &glythSize);

                    glyphRect.top       += ((glyphRect.bottom - glyphRect.top) - glythSize.cy) / 2;
                    glyphRect.bottom    = glyphRect.top + glythSize.cy;
                    glyphRect.right     = glyphRect.left + glythSize.cx;
                    DrawThemeBackground(s_theme, cd->nmcd.hdc, TVP_GLYPH, glyphStates, &glyphRect, nullptr);
                }

                // Text & Icon
                RECT textRect{};
                TreeView_GetItemRect(nmhdr->hwndFrom, hItem, &textRect, TRUE);
                TCHAR textBuffer[MAX_PATH]{};
                TVITEM tvi = {
                    .mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM,
                    .hItem = hItem,
                    .pszText = textBuffer,
                    .cchTextMax = MAX_PATH,
                };
                if (TRUE == TreeView_GetItem(nmhdr->hwndFrom, &tvi)) {
                    const auto elem = reinterpret_cast<PELEM>(GetParam(hItem));
                    if (elem && (elem->uParam & FAVES_FILES) && (elem->uParam & FAVES_PARAM_LINK)) {
                        if (IsFileOpen(elem->link) == TRUE) {
                            ::SelectObject(cd->nmcd.hdc, _pExProp->underlineFont);
                        }
                    }
                    SetBkMode(cd->nmcd.hdc, TRANSPARENT);
                    SetTextColor(cd->nmcd.hdc, _pExProp->themeColors.fgColor);
                    ::DrawText(cd->nmcd.hdc, tvi.pszText, -1, &textRect, DT_SINGLELINE | DT_VCENTER);
                    ::SelectObject(cd->nmcd.hdc, _pExProp->defaultFont);

                    const SIZE iconSize = {
                        .cx = GetSystemMetrics(SM_CXSMICON),
                        .cy = GetSystemMetrics(SM_CYSMICON),
                    };
                    const INT top = (textRect.top + textRect.bottom - iconSize.cy) / 2;
                    const INT left = textRect.left - iconSize.cx - GetSystemMetrics(SM_CXEDGE);
                    if ((_pExProp->bUseSystemIcons == FALSE) || (elem && (elem->uParam & FAVES_PARAM_USERIMAGE))) {
                        ImageList_DrawEx(_hImageList, tvi.iImage, cd->nmcd.hdc, left, top, iconSize.cx, iconSize.cy, CLR_NONE, CLR_NONE, ILD_TRANSPARENT | ILD_SCALE);
                    }
                    else {
                        ImageList_Draw(_hImageListSys, tvi.iImage, cd->nmcd.hdc, left, top, ILD_TRANSPARENT);
                    }
                    return CDRF_SKIPDEFAULT;
                }
                return CDRF_NOTIFYPOSTPAINT;
            }
            case CDDS_POSTPAINT:
                CloseThemeData(s_theme);
                s_theme = nullptr;
                break;
            default:
                break;
            }
        }
    }
    return DefSubclassProc(hwnd, Message, wParam, lParam);
}

LRESULT FavesDialog::runTreeProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message) {
	case WM_GETDLGCODE:
		switch (wParam) {
		case VK_RETURN:
			return DLGC_WANTALLKEYS;
		default:
			break;
		}
		break;
	case WM_KEYDOWN:
		if (VK_ESCAPE == wParam) {
			NppInterface::setFocusToCurrentEdit();
			return TRUE;
		}
		if (wParam == VK_RETURN) {
			HTREEITEM hItem = TreeView_GetSelection(_hTreeCtrl);
			if (OpenTreeViewItem(hItem)) {
				return TRUE;
			}
		}
		if (wParam == VK_DELETE) {
			HTREEITEM hItem = TreeView_GetSelection(_hTreeCtrl);
			DeleteItem(hItem);
		}
		break;
	case WM_LBUTTONDBLCLK: {
		TVHITTESTINFO hti = {
			.pt = {
				.x = GET_X_LPARAM(lParam),
				.y = GET_Y_LPARAM(lParam),
			}
		};

		HTREEITEM hItem = TreeView_HitTest(_hTreeCtrl, &hti);
		if ((hti.flags & TVHT_ONITEM) && OpenTreeViewItem(hItem)) {
			return TRUE;
		}
		break;
	}
    default:
		break;
	}
	
	return ::DefSubclassProc(hwnd, Message, wParam, lParam);
}

BOOL FavesDialog::OpenTreeViewItem(const HTREEITEM hItem)
{
	if (hItem) {
		PELEM	pElem = reinterpret_cast<PELEM>(GetParam(hItem));
		if (pElem) {
			if (!pElem->link.empty()) {
				_peOpenLink = pElem;
				::PostMessage(_hSelf, EXM_OPENLINK, 0, 0);
				return TRUE;
			}
			return FALSE;
		}
	}
	return FALSE;
}

void FavesDialog::tb_cmd(UINT message)
{
	switch (message) {
	case IDM_EX_EXPLORER:
		toggleExplorerDialog();
		break;
	case IDM_EX_LINK_NEW_FILE: {
		TCHAR	TEMP[MAX_PATH] = {};
		::SendMessage(_hParent, NPPM_GETFULLCURRENTPATH, 0, (LPARAM)TEMP);
		if (PathFileExists(TEMP)) {
			AddToFavorties(FALSE, TEMP);
		}
		break;
	}
	case IDM_EX_LINK_NEW_FOLDER: {
		TCHAR	TEMP[MAX_PATH] = {};
		::SendMessage(_hParent, NPPM_GETCURRENTDIRECTORY, 0, (LPARAM)TEMP);
		if (_tcslen(TEMP) != 0) {
			AddToFavorties(TRUE, TEMP);
		}
		break;
	}
	case IDM_EX_LINK_NEW: {
		HTREEITEM	hItem	= TreeView_GetSelection(_hTreeCtrl);
		UINT		root	= ((PELEM)GetParam(hItem))->uParam & FAVES_PARAM;
		if (root == FAVES_SESSIONS) {
			AddSaveSession(hItem, FALSE);
		}
		else {
			NewItem(hItem);
		}
		break;
	}
	case IDM_EX_LINK_EDIT:
		EditItem(TreeView_GetSelection(_hTreeCtrl));
		break;
	case IDM_EX_LINK_DELETE:
		DeleteItem(TreeView_GetSelection(_hTreeCtrl));
		break;
	default:
		break;
	}
}

void FavesDialog::InitialDialog(void)
{
    /* subclass tree */
    ::SetWindowSubclass(_hTreeCtrl, wndDefaultTreeProc, 'tree', reinterpret_cast<DWORD_PTR>(this));

	/* Load Image List */
	_hImageListSys	= GetSmallImageList(TRUE);
	_hImageList		= GetSmallImageList(FALSE);

	/* set image list */
	::SendMessage(_hTreeCtrl, TVM_SETIMAGELIST, TVSIL_NORMAL, (LPARAM)_hImageListSys);

	// set font
	::SendMessage(_hTreeCtrl, WM_SETFONT, (WPARAM)_pExProp->defaultFont, TRUE);

	/* create toolbar */
	_ToolBar.init(_hInst, _hSelf, TB_STANDARD, toolBarIcons, sizeof(toolBarIcons)/sizeof(ToolBarButtonUnit));
	_Rebar.init(_hInst, _hSelf);
	_ToolBar.addToRebar(&_Rebar);
	_Rebar.setIDVisible(REBAR_BAR_TOOLBAR, true);

	/* add new items in list and make reference to items */
    SendMessage(_hTreeCtrl, WM_SETREDRAW, FALSE, 0);
	for (UINT i = 0; i < FAVES_ITEM_MAX; i++) {
		UpdateLink(InsertItem(cFavesItemNames[i], i, i, 0, 0, TVI_ROOT, TVI_LAST, !_vDB[i].children.empty(), &_vDB[i]));
	}
    SendMessage(_hTreeCtrl, WM_SETREDRAW, TRUE, 0);
}

void FavesDialog::SetFont(const HFONT font)
{
	::SendMessage(_hTreeCtrl, WM_SETFONT, (WPARAM)font, TRUE);
}

void FavesDialog::CopyItem(HTREEITEM hItem)
{
	_isCut			= FALSE;
	_hTreeCutCopy	= hItem;
}

void FavesDialog::CutItem(HTREEITEM hItem)
{
	_isCut			= TRUE;
	_hTreeCutCopy	= hItem;
}

void FavesDialog::PasteItem(HTREEITEM hItem)
{
	PELEM	pElem		= (PELEM)GetParam(hItem);
	PELEM	pElemCC		= (PELEM)GetParam(_hTreeCutCopy);

	if ((pElem->uParam & FAVES_PARAM) == (pElemCC->uParam & FAVES_PARAM)) {
		/* add element */
		ItemElement	element;

		if ((_isCut == FALSE) && (TreeView_GetParent(_hTreeCtrl, _hTreeCutCopy) == hItem)) {
			element.name = L"Copy of " + pElemCC->name;
		}
		else {
			element.name = pElemCC->name;
		}

		if (!pElemCC->link.empty()) {
			element.link = pElemCC->link;
		}
		element.uParam		= pElemCC->uParam;
		element.children	= pElemCC->children;

		if (_isCut == TRUE) {
			/* delete item */
			auto parentTreeItem	= TreeView_GetParent(_hTreeCtrl, _hTreeCutCopy);
			auto parentElem     = reinterpret_cast<PELEM>(GetParam(parentTreeItem));

			std::erase_if(parentElem->children, [pElemCC](const auto& elem) {
				return &elem == pElemCC;
			});

			/* update information and delete element */
			UpdateLink(parentTreeItem);
			UpdateNode(parentTreeItem, !parentElem->children.empty());
			ExpandElementsRecursive(parentTreeItem);
		}
		else {
			DuplicateRecursive(&element, pElemCC);
		}

		pElem->children.push_back(element);

		/* update information */
		UpdateLink(hItem);
		UpdateNode(hItem, TRUE);
		ExpandElementsRecursive(hItem);
		if ((pElem->uParam & FAVES_PARAM_EXPAND) == 0) {
			TreeView_Expand(_hTreeCtrl, hItem, TVM_EXPAND | TVE_COLLAPSERESET);
		}

		_hTreeCutCopy = nullptr;
	}
	else
	{
		TCHAR	msgBoxTxt[128];
		_stprintf(msgBoxTxt, _T("Could only be paste into %s"), cFavesItemNames[pElemCC->uParam & FAVES_PARAM]);
		::MessageBox(_hParent, msgBoxTxt, _T("Error"), MB_OK);
	}
}

void FavesDialog::DuplicateRecursive(PELEM pTarget, PELEM pSource)
{
	/* dublicate the content */
	for (SIZE_T i = 0; i < pTarget->children.size(); i++) {
		pTarget->children[i].name = pSource->children[i].name;
		pTarget->children[i].link = pSource->children[i].link;

		DuplicateRecursive(&pTarget->children[i], &pSource->children[i]);
	}
}

void FavesDialog::RefreshTree(HTREEITEM item)
{
    if (item) {
        /* update information */
        HTREEITEM	hParentItem = TreeView_GetParent(_hTreeCtrl, item);
        if (hParentItem != nullptr) {
            UpdateLink(hParentItem);
        }
        UpdateLink(item);
        // expand item
        TreeView_Expand(_hTreeCtrl, item, TVM_EXPAND | TVE_COLLAPSERESET);
    }
}

void FavesDialog::AddToFavorties(BOOL isFolder, LPTSTR szLink)
{
	PropDlg		dlgProp;
	HTREEITEM	hItem		= nullptr;
	BOOL		isOk		= FALSE;
	UINT		root		= (isFolder ? FAVES_FOLDERS : FAVES_FILES);
	LPTSTR		pszName		= (LPTSTR)new TCHAR[MAX_PATH];
	LPTSTR		pszLink		= (LPTSTR)new TCHAR[MAX_PATH];
	LPTSTR		pszDesc		= (LPTSTR)new TCHAR[MAX_PATH];

	/* fill out params */
	pszName[0] = '\0';
	_tcscpy(pszLink, szLink);

	/* create description */
	_stprintf(pszDesc, L"New element in % s", cFavesItemNames[root]);

	/* init properties dialog */
	dlgProp.init(_hInst, _hParent);

	/* select root element */
	dlgProp.setTreeElements(&_vDB[root], (isFolder ? ICON_FOLDER : ICON_FILE));

	/* open dialog */
	if (dlgProp.doDialog(pszName, pszLink, pszDesc, MapPropDlg(root)) == TRUE) {
        ItemElement	element;
        element.name	= pszName;
        element.link	= pszLink;
        element.uParam	= FAVES_PARAM_LINK | root;

        auto groupElem = dlgProp.getSelectedElem();
        groupElem->children.push_back(std::move(element));

        auto item = FindTreeItemByParam(groupElem);
        RefreshTree(item);
    }

	delete [] pszName;
	delete [] pszLink;
	delete [] pszDesc;
}

void FavesDialog::AddToFavorties(BOOL isFolder, std::vector<std::wstring>&& paths)
{
    PropDlg		dlgProp;
    UINT		root = (isFolder ? FAVES_FOLDERS : FAVES_FILES);

    std::wstring name;
    for (auto&& path : paths) {
        if (path.back() == '\\') {
            path.pop_back();
        }
        name += PathFindFileName(path.c_str());
        name += L", ";
    }
    std::wstring desctiption = std::wstring(L"New element in ") + cFavesItemNames[root];

    dlgProp.init(_hInst, _hParent);
    dlgProp.setTreeElements(&_vDB[root], (isFolder ? ICON_FOLDER : ICON_FILE));
    if (dlgProp.doDialog(name.data(), nullptr, desctiption.data(), MapPropDlg(root)) == TRUE) {
        /* get selected item */
        auto groupElem = dlgProp.getSelectedElem();

        if (groupElem != nullptr) {
            for (auto&& path : paths) {
                ItemElement	element;
                element.name = PathFindFileName(path.c_str());
                element.link = std::move(path);
                element.uParam = FAVES_PARAM_LINK | root;
                groupElem->children.push_back(std::move(element));
            }

            auto item = FindTreeItemByParam(groupElem);
            RefreshTree(item);
        }
    }
}


void FavesDialog::AddSaveSession(HTREEITEM hItem, BOOL bSave)
{
	PropDlg		dlgProp;
	HTREEITEM	hParentItem	= nullptr;
	BOOL		isOk		= FALSE;
	PELEM		pElem		= nullptr;
	UINT		root		= FAVES_SESSIONS;
	LPTSTR		pszName		= (LPTSTR)new TCHAR[MAX_PATH];
	LPTSTR		pszLink		= (LPTSTR)new TCHAR[MAX_PATH];
	LPTSTR		pszDesc		= (LPTSTR)new TCHAR[MAX_PATH];

	/* fill out params */
	pszName[0] = '\0';
	pszLink[0] = '\0';

	if (bSave == TRUE) {
		_tcscpy(pszDesc, _T("Save current Session"));
	} else {
		_tcscpy(pszDesc, _T("Add existing Session"));
	}

	/* if hItem is empty, extended dialog is necessary */
	if (hItem == nullptr) {
		/* this is called when notepad menu triggers this function */
		dlgProp.setTreeElements(&_vDB[root], ICON_SESSION, TRUE);
	}
	else {
		/* get group or session information */
		pElem	= (PELEM)GetParam(hItem);
	}

	/* init properties dialog */
	dlgProp.init(_hInst, _hParent);

	/* open dialog */
	if (dlgProp.doDialog(pszName, pszLink, pszDesc, MapPropDlg(root), bSave) == TRUE)
	{
		/* this is called when notepad menu triggers this function */
		if (hItem == nullptr) {
			/* get group name */
            pElem = dlgProp.getSelectedElem();
            hParentItem = FindTreeItemByParam(pElem);

			if (pElem->uParam & FAVES_PARAM_LINK) {
				hItem = FindTreeItemByParam(pElem);
                hParentItem = TreeView_GetParent(_hTreeCtrl, hItem);
			}
		}

		/* if the parent element is LINK element -> replace informations */
		if (pElem->uParam & FAVES_PARAM_LINK) {
			pElem->name	= pszName;
			pElem->link	= pszLink;
		}
		else {
			/* push information back */
			ItemElement	element;
			element.name	= pszName;
			element.link	= pszLink;
			element.uParam	= FAVES_PARAM_LINK | root;
			pElem->children.push_back(std::move(element));
		}

		/* save current session when expected */
		if (bSave == TRUE) {
			::SendMessage(_hParent, NPPM_SAVECURRENTSESSION, 0, (LPARAM)pszLink);
        }

        /* special case for notepad menu trigger */
        if ((hParentItem == nullptr) && (hItem != nullptr)) {
            /* update the session items */
            UpdateLink(hItem);
            TreeView_Expand(_hTreeCtrl, hItem, TVM_EXPAND | TVE_COLLAPSERESET);
        }

        if ((hParentItem != nullptr) && (hItem == nullptr)) {
            /* update the session items */
            UpdateLink(hParentItem);
            TreeView_Expand(_hTreeCtrl, hParentItem, TVM_EXPAND | TVE_COLLAPSERESET);
        }
	}

	delete [] pszName;
	delete [] pszLink;
	delete [] pszDesc;
}

void FavesDialog::NewItem(HTREEITEM hItem)
{
	PropDlg		dlgProp;
	PELEM		pElem		= (PELEM)GetParam(hItem);
	int			root		= (pElem->uParam & FAVES_PARAM);
	BOOL		isOk		= FALSE;
	LPTSTR		pszName		= (LPTSTR)new TCHAR[MAX_PATH];
	LPTSTR		pszLink		= (LPTSTR)new TCHAR[MAX_PATH];
	LPTSTR		pszDesc		= (LPTSTR)new TCHAR[MAX_PATH];

	/* init link and name */
	pszName[0] = '\0';
	pszLink[0] = '\0';

	/* set description text */
	_stprintf(pszDesc, L"New element in % s", cFavesItemNames[root]);

	/* init properties dialog */
	dlgProp.init(_hInst, _hParent);
	while (isOk == FALSE) {
		/* open dialog */
		if (dlgProp.doDialog(pszName, pszLink, pszDesc, MapPropDlg(root)) == TRUE) {
			/* test if name not exist and link exist */
			if (DoesNameNotExist(hItem, nullptr, pszName) == TRUE) {
				isOk = DoesLinkExist(pszLink, root);
			}

			if (isOk == TRUE) {
				ItemElement	element;
				element.name	= pszName;
				element.link	= pszLink;
				element.uParam	= FAVES_PARAM_LINK | root;
				pElem->children.push_back(std::move(element));
			}
		}
		else {
			break;
		}
	}

	if (isOk == TRUE) {
		/* update information */
		if (pElem->uParam & FAVES_PARAM_GROUP) {
			UpdateLink(TreeView_GetParent(_hTreeCtrl, hItem));
		}
		UpdateLink(hItem);

		TreeView_Expand(_hTreeCtrl, hItem, TVM_EXPAND | TVE_COLLAPSERESET);
	}


	delete [] pszName;
	delete [] pszLink;
	delete [] pszDesc;
}

void FavesDialog::EditItem(HTREEITEM hItem)
{
	HTREEITEM	hParentItem = TreeView_GetParent(_hTreeCtrl, hItem);
	PELEM		pElem		= (PELEM)GetParam(hItem);

	if (!(pElem->uParam & FAVES_PARAM_MAIN)) {
		int			root		= (pElem->uParam & FAVES_PARAM);
		BOOL		isOk		= FALSE;
		LPTSTR		pszName		= (LPTSTR)new TCHAR[MAX_PATH];
		LPTSTR		pszLink		= (LPTSTR)new TCHAR[MAX_PATH];
		LPTSTR		pszDesc		= (LPTSTR)new TCHAR[MAX_PATH];
		LPTSTR		pszComm		= (LPTSTR)new TCHAR[MAX_PATH];

		if (pElem->uParam & FAVES_PARAM_GROUP) {
			/* get data of current selected element */
			_tcscpy(pszName, pElem->name.c_str());
			/* rename comment */
			_tcscpy(pszDesc, _T("Properties"));
			_tcscpy(pszComm, _T("Favorites"));

			/* init new dialog */
			NewDlg		dlgNew;


			dlgNew.init(_hInst, _hParent, pszComm);

			/* open dialog */
			while (isOk == FALSE) {
				if (dlgNew.doDialog(pszName, pszDesc) == TRUE) {
					/* test if name not exist */
					isOk = DoesNameNotExist(hParentItem, hItem, pszName);

					if (isOk == TRUE) {
						pElem->name	= pszName;
					}
				}
				else {
					break;
				}
			}
		}
		else if (pElem->uParam & FAVES_PARAM_LINK) {
			/* get data of current selected element */
			_tcscpy(pszName, pElem->name.c_str());
			_tcscpy(pszLink, pElem->link.c_str());
			_tcscpy(pszDesc, _T("Properties"));

			PropDlg		dlgProp;
			dlgProp.init(_hInst, _hParent);
			while (isOk == FALSE) {
				if (dlgProp.doDialog(pszName, pszLink, pszDesc, MapPropDlg(root)) == TRUE) {
					/* test if name not exist and link exist */
					if (DoesNameNotExist(hParentItem, hItem, pszName) == TRUE) {
						isOk = DoesLinkExist(pszLink, root);
					}

					if (isOk == TRUE) {
						pElem->name = pszName;
						pElem->link = pszLink;
					}
				}
				else {
					break;
				}
			}
		}

		/* update text of item */
		if (isOk == TRUE) {
			UpdateLink(hParentItem);
		}

		delete [] pszName;
		delete [] pszLink;
		delete [] pszDesc;
		delete [] pszComm;
	}
}

void FavesDialog::DeleteItem(HTREEITEM hItem)
{
	HTREEITEM	hItemParent	= TreeView_GetParent(_hTreeCtrl, hItem);
	PELEM		pElem		= (PELEM)GetParam(hItem);

	if (pElem && !(pElem->uParam & FAVES_PARAM_MAIN))
	{
		// delete child elements
		pElem->children.clear();

		auto parent = reinterpret_cast<PELEM>(GetParam(hItemParent));
		std::erase_if(parent->children, [pElem](const auto& elem) {
			return &elem == pElem;
		});

		/* update information and delete element */
		TreeView_DeleteItem(_hTreeCtrl, hItem);

		/* update only parent of parent when current item is a group folder */
		if (((PELEM)GetParam(hItemParent))->uParam & FAVES_PARAM_GROUP) {
			UpdateLink(TreeView_GetParent(_hTreeCtrl, hItemParent));
		}
		UpdateLink(hItemParent);
	}
}

void FavesDialog::OpenContext(HTREEITEM hItem, POINT pt)
{
	NewDlg			dlgNew;
	HMENU			hMenu		= nullptr;
	PELEM			pElem		= (PELEM)GetParam(hItem);

	/* get element and level depth */
	if (pElem != nullptr) {
		int		root	= (pElem->uParam & FAVES_PARAM);

		if (pElem->uParam & (FAVES_PARAM_MAIN | FAVES_PARAM_GROUP)) {
			/* create menu and attach one element */
			hMenu = ::CreatePopupMenu();

			if (root != FAVES_SESSIONS) {
				::AppendMenu(hMenu, MF_STRING, FM_NEWLINK, _T("New Link..."));
				::AppendMenu(hMenu, MF_STRING, FM_NEWGROUP, _T("New Group..."));
			}
			else {
				::AppendMenu(hMenu, MF_STRING, FM_ADDSESSION, _T("Add existing Session..."));
				::AppendMenu(hMenu, MF_STRING, FM_SAVESESSION, _T("Save Current Session..."));
				::AppendMenu(hMenu, MF_STRING, FM_NEWGROUP, _T("New Group..."));
			}

			if (pElem->uParam & FAVES_PARAM_GROUP) {
				::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
				::AppendMenu(hMenu, MF_STRING, FM_COPY, _T("Copy"));
				::AppendMenu(hMenu, MF_STRING, FM_CUT, _T("Cut"));
				if (_hTreeCutCopy != nullptr) ::AppendMenu(hMenu, MF_STRING, FM_PASTE, _T("Paste"));
				::AppendMenu(hMenu, MF_STRING, FM_DELETE, _T("Delete"));
				::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
				::AppendMenu(hMenu, MF_STRING, FM_PROPERTIES, _T("Properties..."));
			}
			else if ((pElem->uParam & FAVES_PARAM_MAIN) && (_hTreeCutCopy != nullptr)) {
				::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
				::AppendMenu(hMenu, MF_STRING, FM_PASTE, _T("Paste"));
			}

			/* track menu */
			switch (::TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, _hParent, nullptr)) {
			case FM_NEWLINK:
				NewItem(hItem);
				break;
			case FM_ADDSESSION:
				AddSaveSession(hItem, FALSE);
				break;
			case FM_SAVESESSION:
				AddSaveSession(hItem, TRUE);					
				break;
			case FM_NEWGROUP: {
				BOOL		isOk	= FALSE;
				LPTSTR		pszName	= (LPTSTR)new TCHAR[MAX_PATH];
				LPTSTR		pszDesc = (LPTSTR)new TCHAR[MAX_PATH];
				LPTSTR		pszComm = (LPTSTR)new TCHAR[MAX_PATH];

				pszName[0] = '\0';

				_tcscpy(pszComm, _T("New group in %s"));
				_stprintf(pszDesc, pszComm, cFavesItemNames[root]);

				/* init new dialog */
				dlgNew.init(_hInst, _hParent, _T("Favorites"));

				/* open dialog */
				while (isOk == FALSE) {
					if (dlgNew.doDialog(pszName, pszDesc) == TRUE) {
						/* test if name not exist */
						isOk = DoesNameNotExist(hItem, nullptr, pszName);

						if (isOk == TRUE) {
							ItemElement	element;
							element.name	= pszName;
							element.uParam	= FAVES_PARAM_USERIMAGE | FAVES_PARAM_GROUP | root;
							pElem->children.push_back(std::move(element));

							/* update information */
							if (pElem->uParam & FAVES_PARAM_GROUP) {
								UpdateLink(TreeView_GetParent(_hTreeCtrl, hItem));
							}
							UpdateLink(hItem);
							TreeView_Expand(_hTreeCtrl, hItem, TVM_EXPAND | TVE_COLLAPSERESET);
						}
					}
					else
					{
						isOk = TRUE;
					}
				}

				delete [] pszName;
				delete [] pszDesc;
				delete [] pszComm;
				break;
			}
			case FM_COPY:
				CopyItem(hItem);
				break;
			case FM_CUT:
				CutItem(hItem);
				break;
			case FM_PASTE:
				PasteItem(hItem);
				break;
			case FM_DELETE:
				DeleteItem(hItem);
				break;
			case FM_PROPERTIES:
				EditItem(hItem);
				break;
			}

			/* free resources */
			::DestroyMenu(hMenu);
		}
		else if ((pElem->uParam & FAVES_PARAM_LINK) || (pElem->uParam & FAVES_PARAM_SESSION_CHILD)) {
			/* create menu and attach one element */
			hMenu = ::CreatePopupMenu();

			::AppendMenu(hMenu, MF_STRING, FM_OPEN, _T("Open"));

			if ((root == FAVES_FILES) || (pElem->uParam & FAVES_PARAM_SESSION_CHILD)) {
				::AppendMenu(hMenu, MF_STRING, FM_OPENOTHERVIEW, _T("Open in Other View"));
				::AppendMenu(hMenu, MF_STRING, FM_OPENNEWINSTANCE, _T("Open in New Instance"));
				::AppendMenu(hMenu, MF_STRING, FM_GOTO_FILE_LOCATION, _T("Go to File Location"));
			}
			else if (root == FAVES_SESSIONS) {
				::AppendMenu(hMenu, MF_STRING, FM_ADDTOSESSION, _T("Add to Current Session"));
				::AppendMenu(hMenu, MF_STRING, FM_SAVESESSION, _T("Save Current Session"));
			}

			if (!(pElem->uParam & FAVES_PARAM_SESSION_CHILD)) {
				::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
				::AppendMenu(hMenu, MF_STRING, FM_COPY, _T("Copy"));
				::AppendMenu(hMenu, MF_STRING, FM_CUT, _T("Cut"));
	
	
	
				::AppendMenu(hMenu, MF_STRING, FM_DELETE, _T("Delete"));
				::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
				::AppendMenu(hMenu, MF_STRING, FM_PROPERTIES, _T("Properties..."));
			}

			/* track menu */
			switch (::TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, _hParent, nullptr)) {
			case FM_OPEN:
				OpenLink(pElem);
				break;
			case FM_OPENOTHERVIEW:
				::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)pElem->link.c_str());
				::SendMessage(_hParent, WM_COMMAND, IDM_VIEW_GOTO_ANOTHER_VIEW, 0);
				break;
			case FM_OPENNEWINSTANCE:
			{
				LPTSTR				pszNpp = (LPTSTR)new TCHAR[MAX_PATH];
				// get notepad++.exe path
				::GetModuleFileName(nullptr, pszNpp, MAX_PATH);

				std::wstring 		params = L"-multiInst " + pElem->link;
				::ShellExecute(_hParent, _T("open"), pszNpp, params.c_str(), _T("."), SW_SHOW);

				delete [] pszNpp;
				break;
			}
			case FM_GOTO_FILE_LOCATION: {
				extern ExplorerDialog explorerDlg;

				explorerDlg.gotoFileLocation(pElem->link);
				explorerDlg.doDialog();
				break;
			}
			case FM_ADDTOSESSION:
				_addToSession = TRUE;
				OpenLink(pElem);
				_addToSession = FALSE;
				break;
			case FM_SAVESESSION:
				::SendMessage(_hParent, NPPM_SAVECURRENTSESSION, 0, (LPARAM)pElem->link.c_str());
				DeleteChildren(hItem);
				DrawSessionChildren(hItem);
				break;
			case FM_COPY:
				CopyItem(hItem);
				break;
			case FM_CUT:
				CutItem(hItem);
				break;
			case FM_PASTE:
				PasteItem(hItem);
				break;
			case FM_DELETE:
				DeleteItem(hItem);
				break;
			case FM_PROPERTIES:
				EditItem(hItem);
				break;
			default:
				break;
			}
			/* free resources */
			::DestroyMenu(hMenu);
		}
		else
		{
			::MessageBox(_hParent, _T("Element not found in List!"), _T("Error"), MB_OK);
		}
	}
}


void FavesDialog::UpdateLink(HTREEITEM hParentItem)
{
	BOOL		haveChildren	= FALSE;
	INT			iIconNormal		= 0;
	INT			iIconSelected	= 0;
	INT			iIconOverlayed	= 0;
	INT			root			= 0;
	HTREEITEM	hCurrentItem	= TreeView_GetNextItem(_hTreeCtrl, hParentItem, TVGN_CHILD);

	/* get element list */
	PELEM		parentElement	= (PELEM)GetParam(hParentItem);

	if (parentElement != nullptr) {
		/* sort list */
		SortElementList(parentElement->children);

		/* update elements in parent tree */
		for (SIZE_T i = 0; i < parentElement->children.size(); i++) {
			/* set parent pointer */
			PELEM	pElem	= &parentElement->children[i];

			/* get root */
			root = pElem->uParam & FAVES_PARAM;

			/* initialize children */
			haveChildren		= FALSE;

			if (pElem->uParam & FAVES_PARAM_GROUP)
			{
				iIconNormal		= ICON_GROUP;
				iIconOverlayed	= 0;
				if (pElem->children.size() != 0)
				{
					haveChildren = TRUE;
				}
			}
			else
			{
				/* get icons */
				switch (root) {
				case FAVES_FOLDERS:
					/* get icons and update item */
					ExtractIcons(pElem->link.c_str(), nullptr, DEVT_DIRECTORY, &iIconNormal, &iIconSelected, &iIconOverlayed);
					break;
				case FAVES_FILES:
					/* get icons and update item */
					ExtractIcons(pElem->link.c_str(), nullptr, DEVT_FILE, &iIconNormal, &iIconSelected, &iIconOverlayed);
					break;
				case FAVES_SESSIONS:
					haveChildren	= (0 != ::SendMessage(_hParent, NPPM_GETNBSESSIONFILES, 0, (LPARAM)pElem->link.c_str()));
					iIconNormal		= ICON_SESSION;
					break;
				case FAVES_WEB:
					iIconNormal		= ICON_WEB;
					break;
				default:
					break;
				}
			}
			iIconSelected = iIconNormal;

			/* update or add new item */
			if (hCurrentItem != nullptr) {
				UpdateItem(hCurrentItem, pElem->name, iIconNormal, iIconSelected, iIconOverlayed, 0, haveChildren, pElem);
			}
			else {
				hCurrentItem = InsertItem(pElem->name, iIconNormal, iIconSelected, iIconOverlayed, 0, hParentItem, TVI_LAST, haveChildren, pElem);
			}

			/* control item expand state and correct if necessary */
			BOOL	isTreeExp	= (TreeView_GetItemState(_hTreeCtrl, hCurrentItem, TVIS_EXPANDED) & TVIS_EXPANDED ? TRUE : FALSE);
			BOOL	isItemExp	= (pElem->uParam & FAVES_PARAM_EXPAND ? TRUE : FALSE);

			/* toggle if state is not equal */
			if (isTreeExp != isItemExp) {
				pElem->uParam ^= FAVES_PARAM_EXPAND;
				TreeView_Expand(_hTreeCtrl, hCurrentItem, TVE_TOGGLE);
			}

			/* in any case redraw the session children items */
			if ((pElem->uParam & FAVES_PARAM) == FAVES_SESSIONS) {
				DeleteChildren(hCurrentItem);
				DrawSessionChildren(hCurrentItem);
			}

			hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
		}

		// Update current node
		UpdateNode(hParentItem, !parentElement->children.empty());

		/* delete possible not existed items */
		while (hCurrentItem != nullptr) {
			HTREEITEM	pPrevItem	= hCurrentItem;
			hCurrentItem			= TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
			TreeView_DeleteItem(_hTreeCtrl, pPrevItem);
		}
	}
}

void FavesDialog::UpdateNode(HTREEITEM hItem, BOOL haveChildren)
{
	if (hItem != nullptr) {
		TCHAR TEMP[MAX_PATH] = {};
		TVITEM tvi = {
			.mask		= TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM,
			.hItem		= hItem,
			.pszText	= TEMP,
			.cchTextMax	= MAX_PATH,
		};

		if (TreeView_GetItem(_hTreeCtrl, &tvi) == TRUE) {
			UpdateItem(hItem, TEMP, tvi.iImage, tvi.iSelectedImage, 0, 0, haveChildren, (void*)tvi.lParam);
		}
	}
}

void FavesDialog::DrawSessionChildren(HTREEITEM hItem)
{
	PELEM		pElem			= (PELEM)GetParam(hItem);
	if ((pElem->uParam & FAVES_PARAM_LINK) != FAVES_PARAM_LINK) {
		return;
	}

	pElem->children.clear();

	BOOL hasMissingFile = FALSE;
	auto sessionFiles = NppInterface::getSessionFiles(pElem->link);
	for (const auto &path : sessionFiles)
	{
		ItemElement	element;
		element.uParam = FAVES_SESSIONS | FAVES_PARAM_SESSION_CHILD;
		element.name = path.substr(path.find_last_of(L"\\") + 1);
		element.link = path;
		
		INT		iIconNormal = 0;
		INT		iIconSelected = 0;
		INT		iIconOverlayed = 0;
		if (::PathFileExists(element.link.c_str())) {
			ExtractIcons(element.link.c_str(), nullptr, DEVT_FILE, &iIconNormal, &iIconSelected, &iIconOverlayed);
		}
		else {
			element.uParam |= FAVES_PARAM_USERIMAGE;
			iIconNormal = ICON_MISSING_FILE;
			iIconSelected = iIconNormal;
			hasMissingFile = TRUE;
		}
		pElem->children.push_back(element);
		InsertItem(element.name.c_str(), iIconNormal, iIconSelected, iIconOverlayed, 0, hItem, TVI_LAST, FALSE, &pElem->children.back());
	}

	if (hasMissingFile) {
		pElem->uParam |= FAVES_PARAM_USERIMAGE;
		SetItemIcons(hItem, ICON_WARN_SESSION, ICON_WARN_SESSION, 0);
	}
	else {
		pElem->uParam |= FAVES_PARAM_USERIMAGE;
		SetItemIcons(hItem, ICON_SESSION, ICON_SESSION, 0);		
	}
}


BOOL FavesDialog::DoesNameNotExist(HTREEITEM hParentItem, HTREEITEM hCurrItem, LPTSTR pszName)
{
	BOOL		bRet	= TRUE;
	LPTSTR		TEMP	= (LPTSTR)new TCHAR[MAX_PATH];
	HTREEITEM	hItem	= TreeView_GetChild(_hTreeCtrl, hParentItem);

	while (hItem != nullptr) {
		if (hItem != hCurrItem) {
			GetItemText(hItem, TEMP, MAX_PATH);

			if (_tcscmp(pszName, TEMP) == 0) {
				::MessageBox(_hParent, _T("Name still exists in node!"), _T("Error"), MB_OK);
				bRet = FALSE;
				break;
			}
		}
	
		hItem = TreeView_GetNextItem(_hTreeCtrl, hItem, TVGN_NEXT);
	}

	return bRet;	
}

BOOL FavesDialog::DoesLinkExist(LPTSTR pszLink, int root)
{
	BOOL	bRet = FALSE;

	switch (root) {
	case FAVES_FOLDERS:
		/* test if path exists */
		bRet = ::PathFileExists(pszLink);
		if (bRet == FALSE) {
			::MessageBox(_hParent, _T("Folder doesn't exist!"), _T("Error"), MB_OK);
		}
		break;
	case FAVES_FILES:
	case FAVES_SESSIONS:
		/* test if path exists */
		bRet = ::PathFileExists(pszLink);
		if (bRet == FALSE) {
			::MessageBox(_hParent, _T("File doesn't exist!"), _T("Error"), MB_OK);
		}
		break;
	case FAVES_WEB:
		bRet = TRUE;
		break;
	default:
		::MessageBox(_hParent, _T("Faves element doesn't exist!"), _T("Error"), MB_OK);
		break;
	}

	return bRet;
}


void FavesDialog::OpenLink(PELEM pElem)
{
	if (!pElem->link.empty()) {
		switch (pElem->uParam & FAVES_PARAM) {
		case FAVES_FOLDERS: {
			extern ExplorerDialog		explorerDlg;

			/* two-step to avoid flickering */
			if (explorerDlg.isCreated() == false) {
				explorerDlg.doDialog();
			}

			::SendMessage(explorerDlg.getHSelf(), EXM_OPENDIR, 0, (LPARAM)pElem->link.c_str());

			/* two-step to avoid flickering */
			if (explorerDlg.isVisible() == FALSE) {
				explorerDlg.doDialog();
			}

			::SendMessage(_hParent, NPPM_DMMVIEWOTHERTAB, 0, (LPARAM)"Explorer");
			::SetFocus(explorerDlg.getHSelf());
			break;
		}
		case FAVES_FILES: {
			/* open possible link */
			TCHAR		pszFilePath[MAX_PATH];
			if (ResolveShortCut(pElem->link, pszFilePath, MAX_PATH) == S_OK) {
				::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)pszFilePath);
			} else {
				::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)pElem->link.c_str());
			}
			break;
		}
		case FAVES_WEB:
			::ShellExecute(_hParent, _T("open"), pElem->link.c_str(), nullptr, nullptr, SW_SHOW);
			break;
		case FAVES_SESSIONS:
			if (pElem->uParam & FAVES_PARAM_SESSION_CHILD) {
				NppInterface::doOpen(pElem->link);
			}
			else {
				// Check non-existent files
				auto sessionFiles = NppInterface::getSessionFiles(pElem->link);
				int nonExistentFileCount = 0;
				for (auto &&file : sessionFiles) {
					if (!::PathFileExists(file.c_str())) {
						++nonExistentFileCount;
					}
				}
				if (0 < nonExistentFileCount) {
					if (IDCANCEL == ::MessageBox(_hSelf, StringUtil::format(L"This session has %d non-existent files. Processing will delete all non-existent files in the session. Are you sure you want to continue?", nonExistentFileCount).c_str(), L"Open Session", MB_OKCANCEL | MB_ICONWARNING)) {
						return;
					}
				}

				/* in normal case close files previously */
				if (_addToSession == FALSE) {
					::SendMessage(_hParent, WM_COMMAND, IDM_FILE_CLOSEALL, 0);
					_addToSession = FALSE;
				}
				::SendMessage(_hParent, NPPM_LOADSESSION, 0, (LPARAM)pElem->link.c_str());
			}
			break;
		default:
			break;
		}
	}
}

void FavesDialog::SortElementList(std::vector<ItemElement> & elementList)
{
	std::sort(elementList.begin(), elementList.end(), [](const auto &lhs, const auto &rhs) {
		if ((lhs.uParam & FAVES_PARAM_GROUP) && (rhs.uParam & FAVES_PARAM_LINK)) {
			return true;
		}
		if ((lhs.uParam & FAVES_PARAM_LINK) && (rhs.uParam & FAVES_PARAM_GROUP)) {
			return false;
		}
		return lhs.name < rhs.name;
	});
}

void FavesDialog::ExpandElementsRecursive(HTREEITEM hItem)
{
	HTREEITEM	hCurrentItem	= TreeView_GetNextItem(_hTreeCtrl, hItem, TVGN_CHILD);

	while (hCurrentItem) {
		PELEM	pElem = (PELEM)GetParam(hCurrentItem);

		if (pElem->uParam & FAVES_PARAM_EXPAND) {
			UpdateLink(hCurrentItem);

			/* toggle only the main items, because groups were updated automatically in UpdateLink() */
			if (pElem->uParam & FAVES_PARAM_MAIN) {
				/* if node needs to be expand, delete the indicator first,
				   because TreeView_Expand() function toggles the flag     */
				pElem->uParam ^= FAVES_PARAM_EXPAND;
				TreeView_Expand(_hTreeCtrl, hCurrentItem, TVE_TOGGLE);
			}

			/* traverse into the tree */
			ExpandElementsRecursive(hCurrentItem);
		}

		hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
	}
}


void FavesDialog::ReadSettings(void)
{
	extern TCHAR	configPath[MAX_PATH];
	LPTSTR			readFilePath			= (LPTSTR)new TCHAR[MAX_PATH];
	DWORD			hasRead					= 0;
	HANDLE			hFile					= nullptr;

	/* create root data */
	for (int i = 0; i < FAVES_ITEM_MAX; i++) {
		/* create element list */
		ItemElement	list;
		list.uParam		= FAVES_PARAM_USERIMAGE | FAVES_PARAM_MAIN | i;
		list.name		= cFavesItemNames[i];
		_vDB.push_back(std::move(list));
	}

	/* fill out tree and vDB */
	_tcscpy(readFilePath, configPath);
	_tcscat(readFilePath, FAVES_DATA);

	hFile = ::CreateFile(readFilePath, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (hFile != INVALID_HANDLE_VALUE) {
		DWORD	size = ::GetFileSize(hFile, nullptr);

		if (size != -1) {
			LPTSTR			ptr		= nullptr;
			LPTSTR			data	= (LPTSTR)new TCHAR[size / sizeof(TCHAR)];

			if (data != nullptr) {
				/* read data from file */
				::ReadFile(hFile, data, size, &hasRead, nullptr);

				TCHAR	szBOM = 0xFEFF;
				if (data[0] != szBOM) {
					::MessageBox(_hParent, _T("Error in file 'Favorites.dat'"), _T("Error"), MB_OK | MB_ICONERROR);
				}
				else {
					ptr = data + 1;
					ptr = _tcstok(ptr, _T("\n"));

					/* finaly, fill out the tree and the vDB */
					for (int i = 0; i < FAVES_ITEM_MAX; i++) {
						/* error */
						if (ptr == nullptr) {
							break;
						}

						/* step over name tag */
						if (_tcscmp(cFavesItemNames[i], ptr) == 0) {
							ptr = _tcstok(nullptr, _T("\n"));
							if (ptr == nullptr) {
								break;
							} else if (_tcsstr(ptr, _T("Expand=")) == ptr) {
								if (ptr[7] == '1')
									_vDB[i].uParam |= FAVES_PARAM_EXPAND;
								ptr = _tcstok(nullptr, _T("\n"));
							}
						}
						else {
							::MessageBox(_hSelf, _T("Error in file 'Favorites.dat'"), _T("Error"), MB_OK);
							break;
						}

						/* now read the information */
						ReadElementTreeRecursive(_vDB.begin() + i, &ptr);
					}
				}
				delete [] data;
			}
		}

		::CloseHandle(hFile);
	}

	delete [] readFilePath;
}


void FavesDialog::ReadElementTreeRecursive(ELEM_ITR elem_itr, LPTSTR* ptr)
{
	UINT		defaultParam	= elem_itr->uParam & FAVES_PARAM;
	if (defaultParam == FAVES_WEB) {
		defaultParam |= FAVES_PARAM_USERIMAGE;
	}

	while (1) {
		if (*ptr == nullptr) {
			/* reached end of file -> leave */
			break;
		}
		if (_tcscmp(*ptr, _T("#LINK")) == 0) {
			ItemElement	element;
			/* link is found, get information and fill out the struct */

			/* get element name */
			*ptr = _tcstok(nullptr, _T("\n"));
			if (_tcsstr(*ptr, _T("\tName=")) == *ptr) {
				element.name	= &(*ptr)[6];
				*ptr = _tcstok(nullptr, _T("\n"));
			}
			else {
				::MessageBox(_hSelf, _T("Error in file 'Favorites.dat'\nName in LINK not correct!"), _T("Error"), MB_OK);
			}

			/* get next element link */
			if (_tcsstr(*ptr, _T("\tLink=")) == *ptr) {
				element.link	= &(*ptr)[6];
				*ptr = _tcstok(nullptr, _T("\n"));
			}
			else {
				::MessageBox(_hSelf, _T("Error in file 'Favorites.dat'\nLink in LINK not correct!"), _T("Error"), MB_OK);
			}

			element.uParam	= FAVES_PARAM_LINK | defaultParam;
		
			elem_itr->children.push_back(std::move(element));
		}
		else if ((_tcscmp(*ptr, _T("#GROUP")) == 0) || (_tcscmp(*ptr, _T("#GROUP")) == 0)) {
			ItemElement	element;
			/* group is found, get information and fill out the struct */

			/* get element name */
			*ptr = _tcstok(nullptr, _T("\n"));
			if (_tcsstr(*ptr, _T("\tName=")) == *ptr) {
				element.name	= &(*ptr)[6];
				*ptr = _tcstok(nullptr, _T("\n"));
			}
			else {
				::MessageBox(_hSelf, _T("Error in file 'Favorites.dat'\nName in GROUP not correct!"), _T("Error"), MB_OK);
			}

			BOOL isExpand = false;
			if (_tcsstr(*ptr, _T("\tExpand=")) == *ptr) {
				if ((*ptr)[8] == '1') {
					isExpand = true;
				}
				*ptr = _tcstok(nullptr, _T("\n"));
			}

			element.uParam	= FAVES_PARAM_USERIMAGE | FAVES_PARAM_GROUP | defaultParam;
			if (isExpand) {
				element.uParam |= FAVES_PARAM_EXPAND;
			}
			elem_itr->children.push_back(std::move(element));

			ReadElementTreeRecursive(elem_itr->children.end()-1, ptr);
		}
		else if (_tcscmp(*ptr, _T("")) == 0) {
			/* step over empty lines */
			*ptr = _tcstok(nullptr, _T("\n"));
		}
		else if (_tcscmp(*ptr, _T("#END")) == 0) {
			/* on group end leave the recursion */
			*ptr = _tcstok(nullptr, _T("\n"));
			break;
		}
		else {
			/* there is garbage information/tag */
			break;
		}
	}
}


void FavesDialog::SaveSettings(void)
{
	PELEM			pElem			= nullptr;

	extern TCHAR	configPath[MAX_PATH];
	LPTSTR			saveFilePath	= (LPTSTR)new TCHAR[MAX_PATH];
	DWORD			hasWritten		= 0;
	HANDLE			hFile			= nullptr;

	BYTE			szBOM[]			= {0xFF, 0xFE};

	_tcscpy(saveFilePath, configPath);
	_tcscat(saveFilePath, FAVES_DATA);

	hFile = ::CreateFile(saveFilePath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (hFile != INVALID_HANDLE_VALUE) {
		::WriteFile(hFile, szBOM, sizeof(szBOM), &hasWritten, nullptr);

		/* delete allocated resources */
		HTREEITEM	hItem = TreeView_GetNextItem(_hTreeCtrl, TVI_ROOT, TVGN_CHILD);

		for (int i = 0; i < FAVES_ITEM_MAX; i++) {
			pElem = (PELEM)GetParam(hItem);

			/* store tree */
			std::wstring temp = StringUtil::format(L"%s\nExpand=%i\n\n", cFavesItemNames[i], (_vDB[i].uParam & FAVES_PARAM_EXPAND) == FAVES_PARAM_EXPAND);
			::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, nullptr);
			SaveElementTreeRecursive(pElem, hFile);

			hItem = TreeView_GetNextItem(_hTreeCtrl, hItem, TVGN_NEXT);
		}

		::CloseHandle(hFile);
	}
	else {
		ErrorMessage(GetLastError());
	}

	delete [] saveFilePath;
}


void FavesDialog::SaveElementTreeRecursive(PELEM pElem, HANDLE hFile)
{
	DWORD		hasWritten	= 0;
	PELEM		pElemItr	= nullptr;

	/* delete elements of child items */
	for (SIZE_T i = 0; i < pElem->children.size(); i++) {
		pElemItr = &pElem->children[i];

		if (pElemItr->uParam & FAVES_PARAM_GROUP) {
			::WriteFile(hFile, _T("#GROUP\n"), (DWORD)_tcslen(_T("#GROUP\n")) * sizeof(TCHAR), &hasWritten, nullptr);

			std::wstring temp = StringUtil::format(L"\tName=%s\n", pElemItr->name.c_str());
			::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, nullptr);

			temp = StringUtil::format(L"\tExpand=%i\n\n", (pElemItr->uParam & FAVES_PARAM_EXPAND) == FAVES_PARAM_EXPAND);
			::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, nullptr);

			SaveElementTreeRecursive(pElemItr, hFile);

			::WriteFile(hFile, _T("#END\n\n"), (DWORD)_tcslen(_T("#END\n\n")) * sizeof(TCHAR), &hasWritten, nullptr);
		}
		else if (pElemItr->uParam & FAVES_PARAM_LINK) {
			::WriteFile(hFile, _T("#LINK\n"), (DWORD)_tcslen(_T("#LINK\n")) * sizeof(TCHAR), &hasWritten, nullptr);

			std::wstring temp = StringUtil::format(L"\tName=%s\n", pElemItr->name.c_str());
			::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, nullptr);

			temp = StringUtil::format(L"\tLink=%s\n\n", pElemItr->link.c_str());
			::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, nullptr);
		}
	}
}

void FavesDialog::UpdateColors()
{
	if (nullptr != _hTreeCtrl) {
		TreeView_SetBkColor(_hTreeCtrl, _pExProp->themeColors.bgColor);
		TreeView_SetTextColor(_hTreeCtrl, _pExProp->themeColors.fgColor);
		::InvalidateRect(_hTreeCtrl, nullptr, TRUE);
	}
}
