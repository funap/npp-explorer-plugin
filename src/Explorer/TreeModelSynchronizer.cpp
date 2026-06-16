// The MIT License (MIT)
//
// Copyright (c) 2026 funap
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "TreeModelSynchronizer.h"

#include <algorithm>
#include <windows.h>

#include "ExplorerDialog.h"
#include "ExplorerTasks.h"
#include "Explorer.h"    // FetchIcons, ICON_FOLDER, DEVT_DRIVE, DEVT_DIRECTORY
#include "FileSystemService.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/// Insert a new child folder node into the TreeView.
/// This mirrors ExplorerDialog::InsertChildFolder but is scoped to this TU.
static HTREEITEM InsertChildFolderNode(
    ExplorerDialog& dialog,
    TreeView& treeCtrl,
    std::shared_ptr<ExplorerEntry> entry,
    HTREEITEM parentItem,
    HTREEITEM insertAfter,
    BOOL isDirectory,
    BOOL isHidden,
    BOOL haveChildren)
{
    // Delegate back into the dialog's existing helper which handles icon
    // extraction and shared-ptr lParam bookkeeping.
    return dialog.InsertChildFolder(entry, parentItem, insertAfter, isDirectory, isHidden, haveChildren);
}

// ---------------------------------------------------------------------------
// TreeModelSynchronizer::Synchronize
// ---------------------------------------------------------------------------

void TreeModelSynchronizer::Synchronize(
    ExplorerDialog& dialog,
    TreeView& treeCtrl,
    HTREEITEM hParentItem,
    const std::shared_ptr<ExplorerEntry>& entry,
    Settings* settings,
    ExplorerViewModel& viewModel)
{
    if (FileSystemService::IsUncServerPath(dialog.GetPath(hParentItem))) {
        treeCtrl.SetItemHasChildren(hParentItem, TRUE);
        return;
    }

    HTREEITEM hCurrentChild = treeCtrl.GetNextItem(hParentItem, TVGN_CHILD);

    auto children = entry->Children();

    // Split children into folders and (optionally) files
    std::vector<std::shared_ptr<ExplorerEntry>> folders;
    std::vector<std::shared_ptr<ExplorerEntry>> files;

    for (const auto& child : children) {
        if (child->FSEntry().IsDirectory()) {
            folders.push_back(child);
        } else if (settings->IsUseFullTree()) {
            if (settings->GetFileFilter().match(child->FSEntry().Name())) {
                files.push_back(child);
            }
        }
    }

    // Sort both lists by locale-aware name so the diff is deterministic
    auto byName = [](const std::shared_ptr<ExplorerEntry>& lhs, const std::shared_ptr<ExplorerEntry>& rhs) {
        int result = ::CompareStringEx(
            LOCALE_NAME_USER_DEFAULT,
            LINGUISTIC_IGNOREDIACRITIC | SORT_DIGITSASNUMBERS,
            lhs->FSEntry().Name().c_str(), -1,
            rhs->FSEntry().Name().c_str(), -1,
            NULL, NULL, 0
        );
        return result == CSTR_LESS_THAN;
    };
    std::sort(folders.begin(), folders.end(), byName);
    std::sort(files.begin(),   files.end(),   byName);

    DevType devType = (hParentItem == TVI_ROOT ? DEVT_DRIVE : DEVT_DIRECTORY);

    // Helper: enqueue async icon extraction for a newly placed tree item
    auto enqueueIcon = [&](HTREEITEM hItem) {
        std::wstring currentPath = dialog.GetPath(hItem);
        if (devType == DEVT_DRIVE || settings->IsUseSystemIcons()) {
            viewModel.EnqueueAsyncTask(std::make_unique<TaskTreeViewFetchIcons>(&treeCtrl, hItem, currentPath, devType));
        }
    };

    // Helper: update an existing tree item's data and schedule icon refresh
    auto updateExistingItem = [&](HTREEITEM hItem, const std::shared_ptr<ExplorerEntry>& childEntry) {
        auto* pOld = reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(treeCtrl.GetParam(hItem));
        if (pOld != nullptr) {
            if (*pOld != nullptr && (*pOld)->HasLoadedChildren()) {
                childEntry->SetChildren((*pOld)->Children());
            }
            *pOld = childEntry;
        }

        enqueueIcon(hItem);
    };

    // --- In-order diff loop ---
    // Walk model entries (sorted) and current tree children (also sorted) in parallel.
    // For each model entry either:
    //   * the tree already has it  -> update in place, advance both iterators
    //   * the tree has a different entry that sorts *after* -> insert before it
    //   * the tree has a different entry that sorts *before* -> delete it

    for (const auto* entries_ptr : { &folders, &files }) {
        for (const auto& childEntry : *entries_ptr) {
            std::wstring name = treeCtrl.GetItemText(hCurrentChild);
            if (!name.empty()) {
                // While the current tree child does not match the model entry ...
                while ((name != childEntry->FSEntry().Name()) && (hCurrentChild != nullptr)) {
                    // Check whether the wanted entry appears further down the tree
                    if (dialog.FindFolderAfter(childEntry->FSEntry().Name().c_str(), hCurrentChild) == TRUE) {
                        // Delete the stale (now-removed) child and advance
                        HTREEITEM pPrevItem = hCurrentChild;
                        hCurrentChild = treeCtrl.GetNextItem(hCurrentChild, TVGN_NEXT);
                        treeCtrl.DeleteItem(pPrevItem);
                    } else {
                        // The entry is new - insert it before the current child
                        HTREEITEM pPrevItem = treeCtrl.GetNextItem(hCurrentChild, TVGN_PREVIOUS);
                        if (pPrevItem == nullptr) {
                            hCurrentChild = InsertChildFolderNode(dialog, treeCtrl, childEntry, hParentItem, TVI_FIRST,
                                childEntry->FSEntry().IsDirectory(), childEntry->FSEntry().IsHidden(), childEntry->FSEntry().IsDirectory());
                        } else {
                            hCurrentChild = InsertChildFolderNode(dialog, treeCtrl, childEntry, hParentItem, pPrevItem,
                                childEntry->FSEntry().IsDirectory(), childEntry->FSEntry().IsHidden(), childEntry->FSEntry().IsDirectory());
                        }
                    }

                    if (hCurrentChild != nullptr) {
                        name = treeCtrl.GetItemText(hCurrentChild);
                    }
                }

                // Names match: update the existing item
                updateExistingItem(hCurrentChild, childEntry);
                hCurrentChild = treeCtrl.GetNextItem(hCurrentChild, TVGN_NEXT);

            } else {
                // No more existing children - simply append
                hCurrentChild = InsertChildFolderNode(dialog, treeCtrl, childEntry, hParentItem, TVI_LAST,
                    childEntry->FSEntry().IsDirectory(), childEntry->FSEntry().IsHidden(), childEntry->FSEntry().IsDirectory());
                enqueueIcon(hCurrentChild);
                hCurrentChild = treeCtrl.GetNextItem(hCurrentChild, TVGN_NEXT);
            }
        }
    }

    // Delete any remaining stale children that are past the end of the model list
    while (hCurrentChild != nullptr) {
        HTREEITEM pPrevItem = hCurrentChild;
        hCurrentChild = treeCtrl.GetNextItem(hCurrentChild, TVGN_NEXT);
        treeCtrl.DeleteItem(pPrevItem);
    }

    // Update the parent's "has children" indicator
    if (FileSystemService::IsUncServerPath(dialog.GetPath(hParentItem))) {
        treeCtrl.SetItemHasChildren(hParentItem, TRUE);
    } else {
        treeCtrl.SetItemHasChildren(hParentItem, treeCtrl.GetChild(hParentItem) != nullptr);
    }
}
