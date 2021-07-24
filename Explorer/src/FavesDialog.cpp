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

#include "Explorer.h"
#include "ExplorerDialog.h"
#include "ExplorerResource.h"
#include "NewDlg.h"
#include "resource.h"
#include "NppInterface.h"
#include "StringUtil.h"


extern winVer gWinVersion;

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
		
static LPCTSTR szToolTip[23] = {
	_T("Explorer"),
	_T("Link Current File..."),
	_T("Link Current Folder..."),
	_T("New Link..."),
	_T("Delete Link"),
	_T("Edit Link...")
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
	_hTreeCtrl				= NULL;
	_isCut					= FALSE;
	_hTreeCutCopy			= NULL;
	_addToSession			= FALSE;
}

FavesDialog::~FavesDialog(void)
{
	ImageList_Destroy(_hImageList);
}


void FavesDialog::init(HINSTANCE hInst, NppData nppData, LPTSTR pCurrentElement, ExProp *prop)
{
	_nppData = nppData;
	_pExProp = prop;
	DockingDlgInterface::init(hInst, nppData._nppHandle);

	_pCurrentElement  = pCurrentElement;

	/* init database */
	ReadSettings();
}


void FavesDialog::doDialog(bool willBeShown)
{
    if (!isCreated())
	{
		create(&_data);

		// define the default docking behaviour
		_data.uMask			= DWS_DF_CONT_LEFT | DWS_ICONTAB;
		if (!NLGetText(_hInst, _nppData._nppHandle, _T("Favorites"), _data.pszName, MAX_PATH)) {
			_tcscpy(_data.pszName, _T("Favorites"));
		}
		_data.hIconTab		= (HICON)::LoadImage(_hInst, MAKEINTRESOURCE(IDI_HEART), IMAGE_ICON, 0, 0, LR_LOADMAP3DCOLORS | LR_LOADTRANSPARENT);
		_data.pszModuleName	= getPluginFileName();
		_data.dlgID			= DOCKABLE_FAVORTIES_INDEX;
		::SendMessage(_hParent, NPPM_DMMREGASDCKDLG, 0, (LPARAM)&_data);

		/* Update "Add current..." icons */
		NotifyNewFile();
		ExpandElementsRecursive(TVI_ROOT);
	}

	UpdateColors();
    display(willBeShown);
}


void FavesDialog::SaveSession(void)
{
	AddSaveSession(NULL, TRUE);
}


void FavesDialog::NotifyNewFile(void)
{
	if (isCreated() && isVisible())
	{
		TCHAR	TEMP[MAX_PATH];

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
	switch (Message) 
	{
		case WM_INITDIALOG:
		{
			/* get handle of dialogs */
			_hTreeCtrl		= ::GetDlgItem(_hSelf, IDC_TREE_FOLDER);
			::DestroyWindow(::GetDlgItem(_hSelf, IDC_LIST_FILE));
			::DestroyWindow(::GetDlgItem(_hSelf, IDC_BUTTON_SPLITTER));
			::DestroyWindow(::GetDlgItem(_hSelf, IDC_STATIC_FILTER));
			::DestroyWindow(::GetDlgItem(_hSelf, IDC_COMBO_FILTER));

			InitialDialog();
			break;
		}
		case WM_NOTIFY:
		{
			LPNMHDR		nmhdr = (LPNMHDR)lParam;

			if (nmhdr->hwndFrom == _hTreeCtrl)
			{
				switch (nmhdr->code)
				{
					case NM_RCLICK:
					{
						POINT			pt			= {0};
						TVHITTESTINFO	ht			= {0};
						DWORD			dwpos		= ::GetMessagePos();
						HTREEITEM		hItem		= NULL;

						pt.x = GET_X_LPARAM(dwpos);
						pt.y = GET_Y_LPARAM(dwpos);

						ht.pt = pt;
						::ScreenToClient(_hTreeCtrl, &ht.pt);

						hItem = TreeView_HitTest(_hTreeCtrl, &ht);
						if (hItem != NULL)
						{
							OpenContext(hItem, pt);
						}
						break;
					}
					case TVN_ITEMEXPANDING:
					{
						LPNMTREEVIEW	pnmtv	= (LPNMTREEVIEW) lParam;
						HTREEITEM		hItem	= pnmtv->itemNew.hItem;

						if (hItem != NULL)
						{
							/* get element information */
							PELEM	pElem = (PELEM)pnmtv->itemNew.lParam;

							/* update expand state */
							pElem->uParam ^= FAVES_PARAM_EXPAND;

							// reload session's children
							if (((pElem->uParam & FAVES_PARAM) == FAVES_SESSIONS) &&
								((pElem->uParam & FAVES_PARAM_LINK) == FAVES_PARAM_LINK)) {
								DeleteChildren(hItem);
								DrawSessionChildren(hItem);
							}

							if (!TreeView_GetChild(_hTreeCtrl, hItem))
							{
								if (pElem == NULL)
								{
									/* nothing to do */
								}
								else if (((pElem->uParam & FAVES_PARAM) == FAVES_SESSIONS) && 
										 ((pElem->uParam & FAVES_PARAM_LINK) == FAVES_PARAM_LINK))
								{
									DeleteChildren(hItem);
									DrawSessionChildren(hItem);
								}
								else
								{
									UpdateLink(hItem);
								}
							}
						}
						break;
					}
					case TVN_SELCHANGED:
					{
						HTREEITEM	hItem = TreeView_GetSelection(_hTreeCtrl);

						if (hItem != NULL)
						{
							PELEM	pElem = (PELEM)GetParam(hItem);

							if (pElem != NULL)
							{
								_ToolBar.enable(IDM_EX_LINK_NEW, !(pElem->uParam & FAVES_PARAM_LINK));
								_ToolBar.enable(IDM_EX_LINK_EDIT, !(pElem->uParam & FAVES_PARAM_MAIN));
								_ToolBar.enable(IDM_EX_LINK_DELETE, !(pElem->uParam & FAVES_PARAM_MAIN));
								NotifyNewFile();
							}
							else
							{
								_ToolBar.enable(IDM_EX_LINK_NEW, false);
								_ToolBar.enable(IDM_EX_LINK_EDIT, false);
								_ToolBar.enable(IDM_EX_LINK_DELETE, false);
								NotifyNewFile();
							}
						}						
						break;
					}
					case TVN_GETINFOTIP:
					{
						LPNMTVGETINFOTIP pTip = (LPNMTVGETINFOTIP)lParam;
						HTREEITEM item = pTip->hItem;

						PELEM	pElem = (PELEM)GetParam(item);
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
					case NM_CUSTOMDRAW:
					{
						LPNMTVCUSTOMDRAW lpCD = (LPNMTVCUSTOMDRAW) lParam;

						switch (lpCD->nmcd.dwDrawStage)
						{
							case CDDS_PREPAINT:
							{
								SetWindowLongPtr(_hSelf, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
								return TRUE;
							}
							case CDDS_ITEMPREPAINT:
							{
								HTREEITEM	hItem		= (HTREEITEM)lpCD->nmcd.dwItemSpec;
								PELEM		pElem		= (PELEM)GetParam(hItem);
								BOOL		bUserImage	= FALSE;

								if (pElem) {
									if ((pElem->uParam & FAVES_FILES) && (pElem->uParam & FAVES_PARAM_LINK)) {
										if (IsFileOpen(pElem->link) == TRUE) {
											::SelectObject(lpCD->nmcd.hdc, _pExProp->underlineFont);
										}
									}
								}

								SetWindowLongPtr(_hSelf, DWLP_MSGRESULT, CDRF_NOTIFYPOSTPAINT);
								return TRUE;
							}
							case CDDS_ITEMPOSTPAINT:
							{
								HTREEITEM	hItem		= (HTREEITEM)lpCD->nmcd.dwItemSpec;
								PELEM		pElem		= (PELEM)GetParam(hItem);
								BOOL		bUserImage	= FALSE;

								if (pElem) {
									bUserImage = ((pElem->uParam & FAVES_PARAM_USERIMAGE) == FAVES_PARAM_USERIMAGE);
								}

								if ((_pExProp->bUseSystemIcons == FALSE) || (bUserImage == TRUE))
								{
									RECT	rc			= {0};
									RECT	rcDc		= {0};

									/* get window rect */
									::GetWindowRect(_hTreeCtrl, &rcDc);

									HDC		hMemDc		= ::CreateCompatibleDC(lpCD->nmcd.hdc);
									HBITMAP	hBmp		= ::CreateCompatibleBitmap(lpCD->nmcd.hdc, rcDc.right - rcDc.left, rcDc.bottom - rcDc.top);
									HBITMAP hOldBmp		= (HBITMAP)::SelectObject(hMemDc, hBmp);
									
									COLORREF	bgColor 	= TreeView_GetBkColor(_hTreeCtrl);
									HBRUSH		hBrush		= ::CreateSolidBrush(bgColor);

									/* get item info */
									INT		iIcon		= 0;
									INT		iSelected	= 0;
									INT		iOverlay	= 0;
									GetItemIcons(hItem, &iIcon, &iSelected, &iOverlay);

									/* get item rect */
									TreeView_GetItemRect(_hTreeCtrl, hItem, &rc, TRUE);

									rc.left -= 19;
									rc.right = rc.left + 16;

									/* set transparent mode */
									::SetBkMode(hMemDc, TRANSPARENT);

									::FillRect(hMemDc, &rc, hBrush);
									ImageList_Draw(_hImageList, iIcon, hMemDc, rc.left, rc.top, ILD_NORMAL);

									/* blit text */
									::BitBlt(lpCD->nmcd.hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hMemDc, rc.left, rc.top, SRCCOPY);

									::SelectObject(hMemDc, hOldBmp);
									::DeleteObject(hBrush);
									::DeleteObject(hBmp);
									::DeleteDC(hMemDc);
								}

								::SelectObject(lpCD->nmcd.hdc, _pExProp->defaultFont);

								SetWindowLongPtr(_hSelf, DWLP_MSGRESULT, CDRF_SKIPDEFAULT);
								return TRUE;
							}
							default:
								return FALSE;
						}
						break;
					}
					default:
						break;
				}
			}
			else if ((nmhdr->hwndFrom == _Rebar.getHSelf()) && (nmhdr->code == RBN_CHEVRONPUSHED))
			{
				NMREBARCHEVRON * lpnm = (NMREBARCHEVRON*)nmhdr;
				if (lpnm->wID == REBAR_BAR_TOOLBAR)
				{
					POINT pt;
					pt.x = lpnm->rc.left;
					pt.y = lpnm->rc.bottom;
					ClientToScreen(nmhdr->hwndFrom, &pt);
					tb_cmd(_ToolBar.doPopop(pt));
					return TRUE;
				}
				break;
			}
			else if (nmhdr->code == TTN_GETDISPINFO)
			{
				LPTOOLTIPTEXT lpttt; 

				lpttt = (LPTOOLTIPTEXT)nmhdr; 
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
		case WM_MOVE:
		{
			RECT	rc			= {0};

			/* set position of toolbar */
			getClientRect(rc);
			_Rebar.reSizeTo(rc);

			/* set position of tree control */
			rc.top    += 26;
			rc.bottom -= 26;
			::SetWindowPos(_hTreeCtrl, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

			break;
		}
		case WM_COMMAND:
		{
			// ESC key has been pressed
			if (LOWORD(wParam) == IDCANCEL) {
				NppInterface::setFocusToCurrentEdit();
				return TRUE;
			}

			if ((HWND)lParam == _ToolBar.getHSelf())
			{
				tb_cmd(LOWORD(wParam));
			}
			break;
		}
		case WM_PAINT:
		{
			::RedrawWindow(_ToolBar.getHSelf(), NULL, NULL, TRUE);
			break;
		}
		case WM_DESTROY:
		{
			SaveSettings();
			::DestroyIcon(_data.hIconTab);
			_data.hIconTab = nullptr;

			_ToolBar.destroy();

			/* destroy duplicated handle when we are on W2k machine */
			if (gWinVersion >= WV_W2K) {
				ImageList_Destroy(_hImageListSys);
				_hImageListSys = nullptr;
			}
			break;
		}
		case EXM_OPENLINK:
		{
			OpenLink(_peOpenLink);
			break;
		}
		default:
			DockingDlgInterface::run_dlgProc(Message, wParam, lParam);
			break;
	}

	return FALSE;
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
	case WM_LBUTTONDBLCLK:
	{
		TVHITTESTINFO	hti = {0};

		hti.pt.x = GET_X_LPARAM(lParam);
		hti.pt.y = GET_Y_LPARAM(lParam);

		HTREEITEM hItem = TreeView_HitTest(_hTreeCtrl, &hti);
		if ((hti.flags & TVHT_ONITEM) && OpenTreeViewItem(hItem)) {
			return TRUE;
		}
		break;
	}
	default:
		break;
	}
	
	return ::CallWindowProc(_hDefaultTreeProc, hwnd, Message, wParam, lParam);
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
	switch (message)
	{
		case IDM_EX_EXPLORER:
		{
			toggleExplorerDialog();
			break;
		}
		case IDM_EX_LINK_NEW_FILE:
		{
			TCHAR	TEMP[MAX_PATH] = {};

			::SendMessage(_hParent, NPPM_GETFULLCURRENTPATH, 0, (LPARAM)TEMP);

			if (PathFileExists(TEMP))
			{
				AddToFavorties(FALSE, TEMP);
			}
			break;
		}
		case IDM_EX_LINK_NEW_FOLDER:
		{
			TCHAR	TEMP[MAX_PATH] = {};

			::SendMessage(_hParent, NPPM_GETCURRENTDIRECTORY, 0, (LPARAM)TEMP);

			if (_tcslen(TEMP) != 0)
			{
				AddToFavorties(TRUE, TEMP);
			}
			break;
		}
		case IDM_EX_LINK_NEW:
		{
			HTREEITEM	hItem	= TreeView_GetSelection(_hTreeCtrl);
			UINT		root	= ((PELEM)GetParam(hItem))->uParam & FAVES_PARAM;

			if (root == FAVES_SESSIONS)
			{
				AddSaveSession(hItem, FALSE);
			}
			else
			{
				NewItem(hItem);
			}
			break;
		}
		case IDM_EX_LINK_EDIT:
		{
			EditItem(TreeView_GetSelection(_hTreeCtrl));
			break;
		}
		case IDM_EX_LINK_DELETE:
		{
			DeleteItem(TreeView_GetSelection(_hTreeCtrl));
			break;
		}
		default:
			break;
	}
}

void FavesDialog::InitialDialog(void)
{
	/* change language */
	NLChangeDialog(_hInst, _nppData._nppHandle, _hSelf, _T("Favorites"));

	/* subclass tree */
	::SetWindowLongPtr(_hTreeCtrl, GWLP_USERDATA, (LONG_PTR)this);
	_hDefaultTreeProc = (WNDPROC)::SetWindowLongPtr(_hTreeCtrl, GWLP_WNDPROC, (LONG_PTR)wndDefaultTreeProc);

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
	for (UINT i = 0; i < FAVES_ITEM_MAX; i++)
	{
		UpdateLink(InsertItem(cFavesItemNames[i], i, i, 0, 0, TVI_ROOT, TVI_LAST, !_vDB[i].vElements.empty(), (LPARAM)&_vDB[i]));
	}
}

void FavesDialog::SetFont(const HFONT font)
{
	::SendMessage(_hTreeCtrl, WM_SETFONT, (WPARAM)font, TRUE);
}

HTREEITEM FavesDialog::GetTreeItem(const std::vector<std::wstring> &groupPath) const
{
	if (isCreated())
	{
		HTREEITEM	hItem = TVI_ROOT;
		std::wstring itemText;
		for (const std::wstring &currentPathItem : groupPath) {
			hItem = TreeView_GetChild(_hTreeCtrl, hItem);
			itemText = GetItemText(hItem);

			while (hItem && (itemText != currentPathItem)) {
				hItem = TreeView_GetNextItem(_hTreeCtrl, hItem, TVGN_NEXT);
				itemText = GetItemText(hItem);
			}
		}

		return hItem;
	}

	return nullptr;
}


PELEM FavesDialog::GetElementPointer(const std::vector<std::wstring> &groupPath)
{
	ELEM_ITR	elem_itr = _vDB.begin();
	ELEM_ITR	elem_end = _vDB.end();

	for (size_t i = 0; i < groupPath.size(); i++) {
		for (; elem_itr != elem_end; ++elem_itr) {
			if (elem_itr->name == groupPath[i]){
				break;
			}
		}

		if (i < groupPath.size() - 1) {
			elem_end = elem_itr->vElements.end();
			elem_itr = elem_itr->vElements.begin();
		}
	}

	return &(*elem_itr);
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

	if ((pElem->uParam & FAVES_PARAM) == (pElemCC->uParam & FAVES_PARAM))
	{
		/* add element */
		ItemElement	element;

		if ((_isCut == FALSE) && (TreeView_GetParent(_hTreeCtrl, _hTreeCutCopy) == hItem))
		{
			element.name = L"Copy of " + pElemCC->name;
		}
		else
		{
			element.name = pElemCC->name;
		}

		if (!pElemCC->link.empty())
		{
			element.link = pElemCC->link;
		}
		element.uParam		= pElemCC->uParam;
		element.vElements	= pElemCC->vElements;

		if (_isCut == TRUE)
		{
			/* delete item */
			HTREEITEM	hParentItem	= TreeView_GetParent(_hTreeCtrl, _hTreeCutCopy);
			PELEM		pParentElem = (PELEM)GetParam(hParentItem);

			pParentElem->vElements.erase(pParentElem->vElements.begin()+(pElemCC-&pParentElem->vElements[0]));

			/* update information and delete element */
			UpdateLink(hParentItem);
			UpdateNode(hParentItem, !pParentElem->vElements.empty());
			ExpandElementsRecursive(hParentItem);
		}
		else
		{
			DuplicateRecursive(&element, pElemCC);
		}

		pElem->vElements.push_back(element);

		/* update information */
		UpdateLink(hItem);
		UpdateNode(hItem, TRUE);
		ExpandElementsRecursive(hItem);
		if ((pElem->uParam & FAVES_PARAM_EXPAND) == 0)
		{
			TreeView_Expand(_hTreeCtrl, hItem, TVM_EXPAND | TVE_COLLAPSERESET);
		}

		_hTreeCutCopy = NULL;
	}
	else
	{
		TCHAR	TEMP[128];
		TCHAR	msgBoxTxt[128];

		if (NLGetText(_hInst, _hParent, _T("OnlyPasteInto"), TEMP, 128)) {
			_stprintf(msgBoxTxt, TEMP, cFavesItemNames[pElemCC->uParam & FAVES_PARAM]);
		} else {
			_stprintf(msgBoxTxt, _T("Could only be paste into %s"), cFavesItemNames[pElemCC->uParam & FAVES_PARAM]);
		}
		::MessageBox(_hParent, msgBoxTxt, _T("Error"), MB_OK);
	}
}

void FavesDialog::DuplicateRecursive(PELEM pTarget, PELEM pSource)
{
	/* dublicate the content */
	for (SIZE_T i = 0; i < pTarget->vElements.size(); i++)
	{
		pTarget->vElements[i].name = pSource->vElements[i].name;
		pTarget->vElements[i].link = pSource->vElements[i].link;

		DuplicateRecursive(&pTarget->vElements[i], &pSource->vElements[i]);
	}
}

void FavesDialog::AddToFavorties(BOOL isFolder, LPTSTR szLink)
{
	PropDlg		dlgProp;
	HTREEITEM	hItem		= NULL;
	BOOL		isOk		= FALSE;
	UINT		root		= (isFolder ? FAVES_FOLDERS : FAVES_FILES);
	LPTSTR		pszName		= (LPTSTR)new TCHAR[MAX_PATH];
	LPTSTR		pszLink		= (LPTSTR)new TCHAR[MAX_PATH];
	LPTSTR		pszDesc		= (LPTSTR)new TCHAR[MAX_PATH];
	LPTSTR		pszComm		= (LPTSTR)new TCHAR[MAX_PATH];

	/* fill out params */
	pszName[0] = '\0';
	_tcscpy(pszLink, szLink);

	/* create description */
	if (!NLGetText(_hInst, _nppData._nppHandle, _T("New element in"), pszComm, MAX_PATH)) {
		_tcscpy(pszComm, _T("New element in %s"));
	}
	_stprintf(pszDesc, pszComm, cFavesItemNames[root]);

	/* init properties dialog */
	dlgProp.init(_hInst, _hParent);

	/* select root element */
	dlgProp.setTreeElements(&_vDB[root], (isFolder ? ICON_FOLDER : ICON_FILE));

	while (isOk == FALSE)
	{
		/* open dialog */
		if (dlgProp.doDialog(pszName, pszLink, pszDesc, MapPropDlg(root)) == TRUE)
		{
			/* get selected item */
			const auto groupPath = dlgProp.getGroupPath();
			hItem = GetTreeItem(groupPath);

			/* test if name not exist and link exist */
			if (DoesNameNotExist(hItem, NULL, pszName) == TRUE)
				isOk = DoesLinkExist(pszLink, root);

			if (isOk == TRUE)
			{
				ItemElement	element;
				element.name	= pszName;
				element.link	= pszLink;
				element.uParam	= FAVES_PARAM_LINK | root;
				element.vElements.clear();

				/* push element back */
				PELEM	pElem	= GetElementPointer(groupPath);
				pElem->vElements.push_back(element);
			}
		}
		else
		{
			break;
		}
	}

	if ((isOk == TRUE) && (hItem != NULL))
	{
		/* update information */
		HTREEITEM	hParentItem = TreeView_GetParent(_hTreeCtrl, hItem);
		if (hParentItem != NULL)
		{
			UpdateLink(hParentItem);
		}
		UpdateLink(hItem);

		/* expand item */
		TreeView_Expand(_hTreeCtrl, hItem, TVM_EXPAND | TVE_COLLAPSERESET);
	}

	delete [] pszName;
	delete [] pszLink;
	delete [] pszDesc;
	delete [] pszComm;
}


void FavesDialog::AddSaveSession(HTREEITEM hItem, BOOL bSave)
{
	PropDlg		dlgProp;
	HTREEITEM	hParentItem	= NULL;
	BOOL		isOk		= FALSE;
	PELEM		pElem		= NULL;
	UINT		root		= FAVES_SESSIONS;
	LPTSTR		pszName		= (LPTSTR)new TCHAR[MAX_PATH];
	LPTSTR		pszLink		= (LPTSTR)new TCHAR[MAX_PATH];
	LPTSTR		pszDesc		= (LPTSTR)new TCHAR[MAX_PATH];

	/* fill out params */
	pszName[0] = '\0';
	pszLink[0] = '\0';

	if (bSave == TRUE) {
		if (!NLGetText(_hInst, _nppData._nppHandle, _T("Save current Session"), pszDesc, MAX_PATH)) {
			_tcscpy(pszDesc, _T("Save current Session"));
		}
	} else {
		if (!NLGetText(_hInst, _nppData._nppHandle, _T("Add existing Session"), pszDesc, MAX_PATH)) {
			_tcscpy(pszDesc, _T("Add existing Session"));
		}
	}

	/* if hItem is empty, extended dialog is necessary */
	if (hItem == NULL)
	{
		/* this is called when notepad menu triggers this function */
		dlgProp.setTreeElements(&_vDB[root], ICON_SESSION, TRUE);
	}
	else
	{
		/* get group or session information */
		pElem	= (PELEM)GetParam(hItem);
	}

	/* init properties dialog */
	dlgProp.init(_hInst, _hParent);
	while (isOk == FALSE)
	{
		/* open dialog */
		if (dlgProp.doDialog(pszName, pszLink, pszDesc, MapPropDlg(root), bSave) == TRUE)
		{
			/* this is called when notepad menu triggers this function */
			if (hItem == NULL)
			{
				/* get group name */
				const auto groupPath = dlgProp.getGroupPath();
				hParentItem = GetTreeItem(groupPath);

				/* get pointer by name */
				pElem = GetElementPointer(groupPath);

				if (pElem->uParam & FAVES_PARAM_LINK)
				{
					hItem = hParentItem;
					hParentItem = TreeView_GetParent(_hTreeCtrl, hItem);
				}

				/* test if name not exist and link exist on known hItem */
				if (DoesNameNotExist(hParentItem, hItem, pszName) == TRUE)
					isOk = (bSave == TRUE ? TRUE : DoesLinkExist(pszLink, root));
			}
			else
			{
				/* test if name not exist and link exist on known hItem */
				if (DoesNameNotExist(hItem, NULL, pszName) == TRUE)
					isOk = (bSave == TRUE ? TRUE : DoesLinkExist(pszLink, root));
			}

			if (isOk == TRUE)
			{
				/* if the parent element is LINK element -> replace informations */
				if (pElem->uParam & FAVES_PARAM_LINK)
				{
					pElem->name	= pszName;
					pElem->link	= pszLink;
				}
				else
				{
					/* push information back */
					ItemElement	element;
					element.name	= pszName;
					element.link	= pszLink;
					element.uParam	= FAVES_PARAM_LINK | root;
					element.vElements.clear();

					pElem->vElements.push_back(element);
				}

				/* save current session when expected */
				if (bSave == TRUE)
				{
					::SendMessage(_hParent, NPPM_SAVECURRENTSESSION, 0, (LPARAM)pszLink);
				}
			}
		}
		else
		{
			break;
		}
	}

	if (isOk == TRUE)
	{
		/* special case for notepad menu trigger */
		if ((hParentItem == NULL) && (hItem != NULL))
		{
			/* update the session items */
			UpdateLink(hItem);
			TreeView_Expand(_hTreeCtrl, hItem, TVM_EXPAND | TVE_COLLAPSERESET);
		}

		if ((hParentItem != NULL) && (hItem == NULL))
		{
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
	LPTSTR		pszComm		= (LPTSTR)new TCHAR[MAX_PATH];

	/* init link and name */
	pszName[0] = '\0';
	pszLink[0] = '\0';

	/* set description text */
	if (!NLGetText(_hInst, _nppData._nppHandle, _T("New element in"), pszComm, MAX_PATH)) {
		_tcscpy(pszComm, _T("New element in %s"));
	}
	_stprintf(pszDesc, pszComm, cFavesItemNames[root]);

	/* init properties dialog */
	dlgProp.init(_hInst, _hParent);
	while (isOk == FALSE)
	{
		/* open dialog */
		if (dlgProp.doDialog(pszName, pszLink, pszDesc, MapPropDlg(root)) == TRUE)
		{
			/* test if name not exist and link exist */
			if (DoesNameNotExist(hItem, NULL, pszName) == TRUE)
				isOk = DoesLinkExist(pszLink, root);

			if (isOk == TRUE)
			{
				ItemElement	element;
				element.name	= pszName;
				element.link	= pszLink;
				element.uParam	= FAVES_PARAM_LINK | root;
				element.vElements.clear();

				pElem->vElements.push_back(element);
			}
		}
		else
		{
			break;
		}
	}

	if (isOk == TRUE)
	{
		/* update information */
		if (pElem->uParam & FAVES_PARAM_GROUP)
		{
			UpdateLink(TreeView_GetParent(_hTreeCtrl, hItem));
		}
		UpdateLink(hItem);

		TreeView_Expand(_hTreeCtrl, hItem, TVM_EXPAND | TVE_COLLAPSERESET);
	}


	delete [] pszName;
	delete [] pszLink;
	delete [] pszDesc;
	delete [] pszComm;
}

void FavesDialog::EditItem(HTREEITEM hItem)
{
	HTREEITEM	hParentItem = TreeView_GetParent(_hTreeCtrl, hItem);
	PELEM		pElem		= (PELEM)GetParam(hItem);

	if (!(pElem->uParam & FAVES_PARAM_MAIN))
	{
		int			root		= (pElem->uParam & FAVES_PARAM);
		BOOL		isOk		= FALSE;
		LPTSTR		pszName		= (LPTSTR)new TCHAR[MAX_PATH];
		LPTSTR		pszLink		= (LPTSTR)new TCHAR[MAX_PATH];
		LPTSTR		pszDesc		= (LPTSTR)new TCHAR[MAX_PATH];
		LPTSTR		pszComm		= (LPTSTR)new TCHAR[MAX_PATH];

		if (pElem->uParam & FAVES_PARAM_GROUP)
		{
			/* get data of current selected element */
			_tcscpy(pszName, pElem->name.c_str());
			/* rename comment */
			if (NLGetText(_hInst, _nppData._nppHandle, _T("Properties"), pszDesc, MAX_PATH) == 0) {
				_tcscpy(pszDesc, _T("Properties"));
			}
			if (NLGetText(_hInst, _nppData._nppHandle, _T("Favorites"), pszComm, MAX_PATH) == 0) {
				_tcscpy(pszComm, _T("Favorites"));
			}

			/* init new dialog */
			NewDlg		dlgNew;


			dlgNew.init(_hInst, _hParent, pszComm);

			/* open dialog */
			while (isOk == FALSE)
			{
				if (dlgNew.doDialog(pszName, pszDesc) == TRUE)
				{
					/* test if name not exist */
					isOk = DoesNameNotExist(hParentItem, hItem, pszName);

					if (isOk == TRUE)
					{
						pElem->name	= pszName;
					}
				}
				else
				{
					break;
				}
			}
		}
		else if (pElem->uParam & FAVES_PARAM_LINK) {
			/* get data of current selected element */
			_tcscpy(pszName, pElem->name.c_str());
			_tcscpy(pszLink, pElem->link.c_str());
			if (NLGetText(_hInst, _nppData._nppHandle, _T("Properties"), pszDesc, MAX_PATH) == 0) {
				_tcscpy(pszDesc, _T("Properties"));
			}

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
				else
				{
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
		/* delete child elements */
		DeleteRecursive(pElem);
		((PELEM)GetParam(hItemParent))->vElements.erase(((PELEM)GetParam(hItemParent))->vElements.begin()+(pElem-&((PELEM)GetParam(hItemParent))->vElements[0]));

		/* update information and delete element */
		TreeView_DeleteItem(_hTreeCtrl, hItem);

		/* update only parent of parent when current item is a group folder */
		if (((PELEM)GetParam(hItemParent))->uParam & FAVES_PARAM_GROUP)
		{
			UpdateLink(TreeView_GetParent(_hTreeCtrl, hItemParent));
		}
		UpdateLink(hItemParent);
	}
}

void FavesDialog::DeleteRecursive(PELEM pElem)
{
	/* delete elements of child items */
	for (SIZE_T i = 0; i < pElem->vElements.size(); i++)
	{
		DeleteRecursive(&pElem->vElements[i]);
	}
	pElem->vElements.clear();
}


void FavesDialog::OpenContext(HTREEITEM hItem, POINT pt)
{
	NewDlg			dlgNew;
	HMENU			hMenu		= NULL;
	int				rootLevel	= 0;
	PELEM			pElem		= (PELEM)GetParam(hItem);

	/* get element and level depth */
	if (pElem != NULL)
	{
		int		root	= (pElem->uParam & FAVES_PARAM);

		if (pElem->uParam & (FAVES_PARAM_MAIN | FAVES_PARAM_GROUP))
		{
			/* create menu and attach one element */
			hMenu = ::CreatePopupMenu();

			if (root != FAVES_SESSIONS)
			{
				::AppendMenu(hMenu, MF_STRING, FM_NEWLINK, _T("New Link..."));
				::AppendMenu(hMenu, MF_STRING, FM_NEWGROUP, _T("New Group..."));
			}
			else
			{
				::AppendMenu(hMenu, MF_STRING, FM_ADDSESSION, _T("Add existing Session..."));
				::AppendMenu(hMenu, MF_STRING, FM_SAVESESSION, _T("Save Current Session..."));
				::AppendMenu(hMenu, MF_STRING, FM_NEWGROUP, _T("New Group..."));
			}

			if (pElem->uParam & FAVES_PARAM_GROUP)
			{
				::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
				::AppendMenu(hMenu, MF_STRING, FM_COPY, _T("Copy"));
				::AppendMenu(hMenu, MF_STRING, FM_CUT, _T("Cut"));
				if (_hTreeCutCopy != NULL) ::AppendMenu(hMenu, MF_STRING, FM_PASTE, _T("Paste"));
				::AppendMenu(hMenu, MF_STRING, FM_DELETE, _T("Delete"));
				::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
				::AppendMenu(hMenu, MF_STRING, FM_PROPERTIES, _T("Properties..."));
			}
			else if ((pElem->uParam & FAVES_PARAM_MAIN) && (_hTreeCutCopy != NULL))
			{
				::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
				::AppendMenu(hMenu, MF_STRING, FM_PASTE, _T("Paste"));
			}

			/* change language */
			NLChangeMenu(_hInst, _nppData._nppHandle, hMenu, _T("FavMenu"), MF_BYCOMMAND);
			
			/* track menu */
			switch (::TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, _hParent, NULL))
			{
				case FM_NEWLINK:
				{
					NewItem(hItem);
					break;
				}
				case FM_ADDSESSION:
				{
					AddSaveSession(hItem, FALSE);
					break;
				}
				case FM_SAVESESSION:
				{
					AddSaveSession(hItem, TRUE);					
					break;
				}
				case FM_NEWGROUP:
				{
					BOOL		isOk	= FALSE;
					LPTSTR		pszName	= (LPTSTR)new TCHAR[MAX_PATH];
					LPTSTR		pszDesc = (LPTSTR)new TCHAR[MAX_PATH];
					LPTSTR		pszComm = (LPTSTR)new TCHAR[MAX_PATH];

					pszName[0] = '\0';

					if (!NLGetText(_hInst, _nppData._nppHandle, _T("New group in"), pszComm, MAX_PATH)) {
						_tcscpy(pszComm, _T("New group in %s"));
					}
					_stprintf(pszDesc, pszComm, cFavesItemNames[root]);

					/* init new dialog */
					dlgNew.init(_hInst, _hParent, _T("Favorites"));

					/* open dialog */
					while (isOk == FALSE)
					{
						if (dlgNew.doDialog(pszName, pszDesc) == TRUE)
						{
							/* test if name not exist */
							isOk = DoesNameNotExist(hItem, NULL, pszName);

							if (isOk == TRUE)
							{
								ItemElement	element;
								element.name	= pszName;
								element.link	= std::wstring();
								element.uParam	= FAVES_PARAM_USERIMAGE | FAVES_PARAM_GROUP | root;
								element.vElements.clear();

								pElem->vElements.push_back(element);

								/* update information */
								if (pElem->uParam & FAVES_PARAM_GROUP)
								{
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
				{
					CopyItem(hItem);
					break;
				}
				case FM_CUT:
				{
					CutItem(hItem);
					break;
				}
				case FM_PASTE:
				{
					PasteItem(hItem);
					break;
				}
				case FM_DELETE:
				{
					DeleteItem(hItem);
					break;
				}
				case FM_PROPERTIES:
				{
					EditItem(hItem);
					break;
				}
			}

			/* free resources */
			::DestroyMenu(hMenu);
		}
		else if ((pElem->uParam & FAVES_PARAM_LINK) || (pElem->uParam & FAVES_PARAM_SESSION_CHILD))
		{
			/* create menu and attach one element */
			hMenu = ::CreatePopupMenu();

			::AppendMenu(hMenu, MF_STRING, FM_OPEN, _T("Open"));

			if ((root == FAVES_FILES) || (pElem->uParam & FAVES_PARAM_SESSION_CHILD))
			{
				::AppendMenu(hMenu, MF_STRING, FM_OPENOTHERVIEW, _T("Open in Other View"));
				::AppendMenu(hMenu, MF_STRING, FM_OPENNEWINSTANCE, _T("Open in New Instance"));
				::AppendMenu(hMenu, MF_STRING, FM_GOTO_FILE_LOCATION, _T("Go to File Location"));
			}
			else if (root == FAVES_SESSIONS)
			{
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
			/* change language */
			NLChangeMenu(_hInst, _nppData._nppHandle, hMenu, _T("FavMenu"), MF_BYCOMMAND);

			/* track menu */
			switch (::TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, _hParent, NULL))
			{
				case FM_OPEN:
				{
					OpenLink(pElem);
					break;
				}
				case FM_OPENOTHERVIEW:
				{
					::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)pElem->link.c_str());
					::SendMessage(_hParent, WM_COMMAND, IDM_VIEW_GOTO_ANOTHER_VIEW, 0);
					break;
				}
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
				case FM_GOTO_FILE_LOCATION:
				{
					extern ExplorerDialog explorerDlg;

					explorerDlg.gotoFileLocation(pElem->link);
					explorerDlg.doDialog();
					break;
				}
				case FM_ADDTOSESSION:
				{
					_addToSession = TRUE;
					OpenLink(pElem);
					_addToSession = FALSE;
					break;
				}
				case FM_SAVESESSION:
				{
					::SendMessage(_hParent, NPPM_SAVECURRENTSESSION, 0, (LPARAM)pElem->link.c_str());
					DeleteChildren(hItem);
					DrawSessionChildren(hItem);
					break;
				}
				case FM_COPY:
				{
					CopyItem(hItem);
					break;
				}
				case FM_CUT:
				{
					CutItem(hItem);
					break;
				}
				case FM_PASTE:
				{
					PasteItem(hItem);
					break;
				}
				case FM_DELETE:
				{
					DeleteItem(hItem);
					break;
				}
				case FM_PROPERTIES:
				{
					EditItem(hItem);
					break;
				}
				default:
					break;
			}
			/* free resources */
			::DestroyMenu(hMenu);
		}
		else
		{
			if (NLMessageBox(_hInst, _hParent, _T("MsgBox NotInList"), MB_OK) == FALSE)
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

	if (parentElement != NULL)
	{
		/* sort list */
		SortElementList(parentElement->vElements);

		/* update elements in parent tree */
		for (SIZE_T i = 0; i < parentElement->vElements.size(); i++)
		{
			/* set parent pointer */
			PELEM	pElem	= &parentElement->vElements[i];

			/* get root */
			root = pElem->uParam & FAVES_PARAM;

			/* initialize children */
			haveChildren		= FALSE;

			if (pElem->uParam & FAVES_PARAM_GROUP)
			{
				iIconNormal		= ICON_GROUP;
				iIconOverlayed	= 0;
				if (pElem->vElements.size() != 0)
				{
					haveChildren = TRUE;
				}
			}
			else
			{
				/* get icons */
				switch (root)
				{
					case FAVES_FOLDERS:
					{
						/* get icons and update item */
						ExtractIcons(pElem->link.c_str(), nullptr, DEVT_DIRECTORY, &iIconNormal, &iIconSelected, &iIconOverlayed);
						break;
					}
					case FAVES_FILES:
					{
						/* get icons and update item */
						ExtractIcons(pElem->link.c_str(), nullptr, DEVT_FILE, &iIconNormal, &iIconSelected, &iIconOverlayed);
						break;
					}
					case FAVES_SESSIONS:
					{
						haveChildren	= (0 != ::SendMessage(_hParent, NPPM_GETNBSESSIONFILES, 0, (LPARAM)pElem->link.c_str()));
						iIconNormal		= ICON_SESSION;
						break;
					}
					case FAVES_WEB:
					{
						iIconNormal		= ICON_WEB;
						break;
					}
				}
			}
			iIconSelected = iIconNormal;

			/* update or add new item */
			if (hCurrentItem != NULL)
			{
				UpdateItem(hCurrentItem, pElem->name, iIconNormal, iIconSelected, iIconOverlayed, 0, haveChildren, (LPARAM)pElem);
			}
			else
			{
				hCurrentItem = InsertItem(pElem->name, iIconNormal, iIconSelected, iIconOverlayed, 0, hParentItem, TVI_LAST, haveChildren, (LPARAM)pElem);
			}

			/* control item expand state and correct if necessary */
			BOOL	isTreeExp	= (TreeView_GetItemState(_hTreeCtrl, hCurrentItem, TVIS_EXPANDED) & TVIS_EXPANDED ? TRUE : FALSE);
			BOOL	isItemExp	= (pElem->uParam & FAVES_PARAM_EXPAND ? TRUE : FALSE);

			/* toggle if state is not equal */
			if (isTreeExp != isItemExp)
			{
				pElem->uParam ^= FAVES_PARAM_EXPAND;
				TreeView_Expand(_hTreeCtrl, hCurrentItem, TVE_TOGGLE);
			}

			/* in any case redraw the session children items */
			if ((pElem->uParam & FAVES_PARAM) == FAVES_SESSIONS)
			{
				DeleteChildren(hCurrentItem);
				DrawSessionChildren(hCurrentItem);
			}

			hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
		}

		// Update current node
		UpdateNode(hParentItem, !parentElement->vElements.empty());

		/* delete possible not existed items */
		while (hCurrentItem != NULL)
		{
			HTREEITEM	pPrevItem	= hCurrentItem;
			hCurrentItem			= TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
			TreeView_DeleteItem(_hTreeCtrl, pPrevItem);
		}
	}
}

void FavesDialog::UpdateNode(HTREEITEM hItem, BOOL haveChildren)
{
	if (hItem != NULL)
	{
		TCHAR	TEMP[MAX_PATH] = {};

		TVITEM			tvi;
		tvi.mask		= TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
		tvi.hItem		= hItem;
		tvi.pszText		= TEMP;
		tvi.cchTextMax	= MAX_PATH;

		if (TreeView_GetItem(_hTreeCtrl, &tvi) == TRUE)
		{
			UpdateItem(hItem, TEMP, tvi.iImage, tvi.iSelectedImage, 0, 0, haveChildren, tvi.lParam);
		}
	}
}

void FavesDialog::DrawSessionChildren(HTREEITEM hItem)
{
	INT			i				= 0;
	INT			docCnt			= 0;
	LPTSTR		*ppszFileNames	= NULL;
	PELEM		pElem			= (PELEM)GetParam(hItem);

	if ((pElem->uParam & FAVES_PARAM_LINK) != FAVES_PARAM_LINK) {
		return;
	}

	pElem->vElements.clear();

	BOOL		hasMissingFile = FALSE;

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
		pElem->vElements.push_back(element);
		HTREEITEM inserted = InsertItem(element.name.c_str(), iIconNormal, iIconSelected, iIconOverlayed, 0, hItem, TVI_LAST, FALSE, (LPARAM)&pElem->vElements.back());
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

	while (hItem != NULL)
	{
		if (hItem != hCurrItem)
		{
			GetItemText(hItem, TEMP, MAX_PATH);

			if (_tcscmp(pszName, TEMP) == 0)
			{
				if (NLMessageBox(_hInst, _hParent, _T("MsgBox ExistsInNode"), MB_OK) == FALSE)
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

	switch (root)
	{
		case FAVES_FOLDERS:
		{
			/* test if path exists */
			bRet = ::PathFileExists(pszLink);
			if (bRet == FALSE) {
				if (NLMessageBox(_hInst, _hParent, _T("MsgBox FolderMiss"), MB_OK) == FALSE)
					::MessageBox(_hParent, _T("Folder doesn't exist!"), _T("Error"), MB_OK);
			}
			break;
		}
		case FAVES_FILES:
		case FAVES_SESSIONS:
		{
			/* test if path exists */
			bRet = ::PathFileExists(pszLink);
			if (bRet == FALSE) {
				if (NLMessageBox(_hInst, _hParent, _T("MsgBox FileMiss"), MB_OK) == FALSE)
					::MessageBox(_hParent, _T("File doesn't exist!"), _T("Error"), MB_OK);
			}
			break;
		}
		case FAVES_WEB:
		{
			bRet = TRUE;
			break;
		}
		default:
			if (NLMessageBox(_hInst, _hParent, _T("MsgBox FavesElemMiss"), MB_OK) == FALSE)
				::MessageBox(_hParent, _T("Faves element doesn't exist!"), _T("Error"), MB_OK);
			break;
	}

	return bRet;
}


void FavesDialog::OpenLink(PELEM pElem)
{
	if (!pElem->link.empty())
	{
		switch (pElem->uParam & FAVES_PARAM)
		{
			case FAVES_FOLDERS:
			{
				extern ExplorerDialog		explorerDlg;

				/* two-step to avoid flickering */
				if (explorerDlg.isCreated() == false)
					explorerDlg.doDialog();

				::SendMessage(explorerDlg.getHSelf(), EXM_OPENDIR, 0, (LPARAM)pElem->link.c_str());

				/* two-step to avoid flickering */
				if (explorerDlg.isVisible() == FALSE)
					explorerDlg.doDialog();

				::SendMessage(_hParent, NPPM_DMMVIEWOTHERTAB, 0, (LPARAM)"Explorer");
				::SetFocus(explorerDlg.getHSelf());
				break;
			}
			case FAVES_FILES:
			{
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
			{
				::ShellExecute(_hParent, _T("open"), pElem->link.c_str(), NULL, NULL, SW_SHOW);
				break;
			}
			case FAVES_SESSIONS:
			{
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
			}
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

	while (hCurrentItem)
	{
		PELEM	pElem = (PELEM)GetParam(hCurrentItem);

		if (pElem->uParam & FAVES_PARAM_EXPAND)
		{
			UpdateLink(hCurrentItem);

			/* toggle only the main items, because groups were updated automatically in UpdateLink() */
			if (pElem->uParam & FAVES_PARAM_MAIN)
			{
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
	ItemElement	list;
	extern TCHAR	configPath[MAX_PATH];
	LPTSTR			readFilePath			= (LPTSTR)new TCHAR[MAX_PATH];
	DWORD			hasRead					= 0;
	HANDLE			hFile					= NULL;

	/* create root data */
	for (int i = 0; i < FAVES_ITEM_MAX; i++)
	{
		/* create element list */
		list.uParam		= FAVES_PARAM_USERIMAGE | FAVES_PARAM_MAIN | i;
		list.name		= cFavesItemNames[i];
		list.link		= std::wstring();
		list.vElements.clear();

		_vDB.push_back(list);
	}

	/* fill out tree and vDB */
	_tcscpy(readFilePath, configPath);
	_tcscat(readFilePath, FAVES_DATA);

	hFile = ::CreateFile(readFilePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		DWORD	size = ::GetFileSize(hFile, NULL);

		if (size != -1)
		{
			LPTSTR			ptr		= NULL;
			LPTSTR			data	= (LPTSTR)new TCHAR[size / sizeof(TCHAR)];

			if (data != NULL)
			{
				/* read data from file */
				::ReadFile(hFile, data, size, &hasRead, NULL);

				TCHAR	szBOM = 0xFEFF;
				if (data[0] != szBOM)
				{
					::MessageBox(_nppData._nppHandle, _T("Error in file 'Favorites.dat'"), _T("Error"), MB_OK | MB_ICONERROR);
				}
				else
				{
					ptr = data + 1;
					ptr = _tcstok(ptr, _T("\n"));

					/* finaly, fill out the tree and the vDB */
					for (int i = 0; i < FAVES_ITEM_MAX; i++)
					{
						/* error */
						if (ptr == NULL)
							break;

						/* step over name tag */
						if (_tcscmp(cFavesItemNames[i], ptr) == 0)
						{
							ptr = _tcstok(NULL, _T("\n"));
							if (ptr == NULL) {
								break;
							} else if (_tcsstr(ptr, _T("Expand=")) == ptr) {
								if (ptr[7] == '1')
									_vDB[i].uParam |= FAVES_PARAM_EXPAND;
								ptr = _tcstok(NULL, _T("\n"));
							}
						}
						else
						{
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
	ItemElement	element;
	LPTSTR		pszPos			= NULL;
	 UINT		defaultParam	= elem_itr->uParam & FAVES_PARAM;
	if (defaultParam == FAVES_WEB) {
		defaultParam |= FAVES_PARAM_USERIMAGE;
	}

	while (1)
	{
		if (*ptr == NULL)
		{
			/* reached end of file -> leave */
			break;
		}
		if (_tcscmp(*ptr, _T("#LINK")) == 0)
		{
			/* link is found, get information and fill out the struct */

			/* get element name */
			*ptr = _tcstok(NULL, _T("\n"));
			if (_tcsstr(*ptr, _T("\tName=")) == *ptr)
			{
				element.name	= &(*ptr)[6];
				*ptr = _tcstok(NULL, _T("\n"));
			}
			else {
				::MessageBox(_hSelf, _T("Error in file 'Favorites.dat'\nName in LINK not correct!"), _T("Error"), MB_OK);
			}

			/* get next element link */
			if (_tcsstr(*ptr, _T("\tLink=")) == *ptr)
			{
				element.link	= &(*ptr)[6];
				*ptr = _tcstok(NULL, _T("\n"));
			}
			else {
				::MessageBox(_hSelf, _T("Error in file 'Favorites.dat'\nLink in LINK not correct!"), _T("Error"), MB_OK);
			}

			element.uParam	= FAVES_PARAM_LINK | defaultParam;
			element.vElements.clear();
			
			elem_itr->vElements.push_back(element);
		}
		else if ((_tcscmp(*ptr, _T("#GROUP")) == 0) || (_tcscmp(*ptr, _T("#GROUP")) == 0))
		{
			/* group is found, get information and fill out the struct */

			/* get element name */
			*ptr = _tcstok(NULL, _T("\n"));
			if (_tcsstr(*ptr, _T("\tName=")) == *ptr)
			{
				element.name	= &(*ptr)[6];
				*ptr = _tcstok(NULL, _T("\n"));
			}
			else {
				::MessageBox(_hSelf, _T("Error in file 'Favorites.dat'\nName in GROUP not correct!"), _T("Error"), MB_OK);
			}

			BOOL isExpand = false;
			if (_tcsstr(*ptr, _T("\tExpand=")) == *ptr) {
				if ((*ptr)[8] == '1') {
					isExpand = true;
				}
				*ptr = _tcstok(NULL, _T("\n"));
			}

			element.link	= std::wstring();
			element.uParam	= FAVES_PARAM_USERIMAGE | FAVES_PARAM_GROUP | defaultParam;
			if (isExpand) {
				element.uParam |= FAVES_PARAM_EXPAND;
			}
			element.vElements.clear();

			elem_itr->vElements.push_back(element);

			ReadElementTreeRecursive(elem_itr->vElements.end()-1, ptr);
		}
		else if (_tcscmp(*ptr, _T("")) == 0)
		{
			/* step over empty lines */
			*ptr = _tcstok(NULL, _T("\n"));
		}
		else if (_tcscmp(*ptr, _T("#END")) == 0)
		{
			/* on group end leave the recursion */
			*ptr = _tcstok(NULL, _T("\n"));
			break;
		}
		else
		{
			/* there is garbage information/tag */
			break;
		}
	}
}


void FavesDialog::SaveSettings(void)
{
	PELEM			pElem			= NULL;

	extern TCHAR	configPath[MAX_PATH];
	LPTSTR			saveFilePath	= (LPTSTR)new TCHAR[MAX_PATH];
	DWORD			hasWritten		= 0;
	HANDLE			hFile			= NULL;

	BYTE			szBOM[]			= {0xFF, 0xFE};

	_tcscpy(saveFilePath, configPath);
	_tcscat(saveFilePath, FAVES_DATA);

	hFile = ::CreateFile(saveFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		::WriteFile(hFile, szBOM, sizeof(szBOM), &hasWritten, NULL);

		/* delete allocated resources */
		HTREEITEM	hItem = TreeView_GetNextItem(_hTreeCtrl, TVI_ROOT, TVGN_CHILD);

		for (int i = 0; i < FAVES_ITEM_MAX; i++)
		{
			pElem = (PELEM)GetParam(hItem);

			/* store tree */
			std::wstring temp = StringUtil::format(L"%s\nExpand=%i\n\n", cFavesItemNames[i], (_vDB[i].uParam & FAVES_PARAM_EXPAND) == FAVES_PARAM_EXPAND);
			::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, NULL);
			SaveElementTreeRecursive(pElem, hFile);

			/* delete tree */
			DeleteRecursive(pElem);

			hItem = TreeView_GetNextItem(_hTreeCtrl, hItem, TVGN_NEXT);
		}

		::CloseHandle(hFile);
	}
	else
	{
		ErrorMessage(GetLastError());
	}

	delete [] saveFilePath;
}


void FavesDialog::SaveElementTreeRecursive(PELEM pElem, HANDLE hFile)
{
	DWORD		hasWritten	= 0;
	SIZE_T		size		= 0;
	LPTSTR		temp		= NULL;
	PELEM		pElemItr	= NULL;

	/* delete elements of child items */
	for (SIZE_T i = 0; i < pElem->vElements.size(); i++)
	{
		pElemItr = &pElem->vElements[i];

		if (pElemItr->uParam & FAVES_PARAM_GROUP)
		{
			::WriteFile(hFile, _T("#GROUP\n"), (DWORD)_tcslen(_T("#GROUP\n")) * sizeof(TCHAR), &hasWritten, NULL);

			std::wstring temp = StringUtil::format(L"\tName=%s\n", pElemItr->name.c_str());
			::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, NULL);

			temp = StringUtil::format(L"\tExpand=%i\n\n", (pElemItr->uParam & FAVES_PARAM_EXPAND) == FAVES_PARAM_EXPAND);
			::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, NULL);

			SaveElementTreeRecursive(pElemItr, hFile);

			::WriteFile(hFile, _T("#END\n\n"), (DWORD)_tcslen(_T("#END\n\n")) * sizeof(TCHAR), &hasWritten, NULL);
		}
		else if (pElemItr->uParam & FAVES_PARAM_LINK)
		{
			::WriteFile(hFile, _T("#LINK\n"), (DWORD)_tcslen(_T("#LINK\n")) * sizeof(TCHAR), &hasWritten, NULL);

			std::wstring temp = StringUtil::format(L"\tName=%s\n", pElemItr->name.c_str());
			::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, NULL);

			temp = StringUtil::format(L"\tLink=%s\n\n", pElemItr->link.c_str());
			::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, NULL);
		}
	}
}

void FavesDialog::UpdateColors()
{
	COLORREF fgColor = NppInterface::getEditorDefaultForegroundColor();
	COLORREF bgColor = NppInterface::getEditorDefaultBackgroundColor();

	if (NULL != _hTreeCtrl) {
		TreeView_SetBkColor(_hTreeCtrl, bgColor);
		TreeView_SetTextColor(_hTreeCtrl, fgColor);
		::InvalidateRect(_hTreeCtrl, NULL, TRUE);
	}
}
