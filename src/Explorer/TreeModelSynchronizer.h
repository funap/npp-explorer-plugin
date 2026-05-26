// The MIT License (MIT)
//
// Copyright (c) 2024 funap
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

#pragma once

#include <memory>
#include <vector>
#include <windows.h>
#include <commctrl.h>

#include "ExplorerModel.h"
#include "TreeView.h"
#include "WorkerThread.h"
#include "Settings.h"

// Forward declaration to avoid circular dependency
// (ExplorerDialog.h includes TreeModelSynchronizer.h via forward declaration)
class ExplorerDialog;

/// @brief Stateless helper that synchronizes a directory's children from the
///        ExplorerModel into the Win32 TreeView control.
///
/// Separating this logic from ExplorerDialog gives us:
///   - A single, testable place that owns the "diff-and-patch" algorithm.
///   - A slimmer ExplorerDialog whose OnEntryUpdated is just a one-liner
///     delegation.
class TreeModelSynchronizer {
public:
    TreeModelSynchronizer() = delete;

    /// @brief Synchronize the children of @p hParentItem in @p treeCtrl with the
    ///        children stored in @p entry.
    ///
    /// The algorithm is an in-order diff: it walks the existing HTREEITEM
    /// children and the sorted model children side-by-side, inserting missing
    /// items, deleting removed items, and updating unchanged items.
    ///
    /// @param dialog       Back-pointer used to resolve paths from HTREEITEMs
    ///                     and to post async icon-extraction tasks.
    /// @param treeCtrl     The Win32 TreeView wrapper.
    /// @param hParentItem  The parent node whose children are to be updated.
    /// @param entry        The ExplorerEntry whose children represent the
    ///                     desired state of the tree node.
    /// @param settings     Plugin settings (hidden-files, full-tree mode, etc.).
    /// @param workerThread Worker thread used to dispatch icon-extraction tasks.
    static void Synchronize(
        ExplorerDialog& dialog,
        TreeView& treeCtrl,
        HTREEITEM hParentItem,
        const std::shared_ptr<ExplorerEntry>& entry,
        Settings* settings,
        WorkerThread& workerThread);
};

