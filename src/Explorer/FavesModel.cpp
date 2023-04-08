/*
  The MIT License (MIT)

  Copyright (c) 2023 funap

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/
#include "FavesModel.h"

// ----------------------------------------------------------------------------
// FavesModel
// ----------------------------------------------------------------------------

FavesModel::FavesModel()
    : m_folders (std::make_unique<FavesItem>(nullptr, FAVES_FOLDER,  L"[Folders]"))
    , m_files   (std::make_unique<FavesItem>(nullptr, FAVES_FILE,    L"[Files]"))
    , m_webs    (std::make_unique<FavesItem>(nullptr, FAVES_WEB,     L"[Web]"))
    , m_sessions(std::make_unique<FavesItem>(nullptr, FAVES_SESSION, L"[Sessions]"))
{
}

FavesModel::~FavesModel()
{
}

void FavesModel::Clear()
{
    m_folders->ClearChildren();
    m_files->ClearChildren();
    m_webs->ClearChildren();
    m_sessions->ClearChildren();
}

FavesItemPtr FavesModel::FolderRoot() const
{
    return m_folders.get();
}

FavesItemPtr FavesModel::FileRoot() const
{
    return m_files.get();
}

FavesItemPtr FavesModel::WebRoot() const
{
    return m_webs.get();
}

FavesItemPtr FavesModel::SessionRoot() const
{
    return m_sessions.get();
}

// ----------------------------------------------------------------------------
// FavesItem
// ----------------------------------------------------------------------------
FavesItem::FavesItem(const FavesItemPtr parent, FavesType type, const std::wstring& name, const std::wstring& link)
    : m_parent(parent)
    , m_type(type)
    , m_name(name)
    , m_link(link)
    , m_isExpanded(FALSE)
    , m_children{}
{
}

FavesItem::~FavesItem()
{
}

FavesItemPtr FavesItem::Root()
{
    if (IsRoot()) {
        return this;
    }
    return m_parent->Root();
}

FavesType FavesItem::Type() const
{
    return m_type;
}

const std::wstring& FavesItem::Name() const
{
    return m_name;
}

const std::wstring& FavesItem::Link() const
{
    return m_link;
}

BOOL FavesItem::IsExpanded() const
{
    return m_isExpanded;
}

VOID FavesItem::IsExpanded(BOOL isExpanded)
{
    m_isExpanded = isExpanded;
}

BOOL FavesItem::IsNodeDescendant(const FavesItem* anotherNode) const
{
    if (nullptr == anotherNode) {
        return FALSE;
    }
    if (this == anotherNode) {
        return TRUE;
    }
    return IsNodeDescendant(anotherNode->m_parent);
}

VOID FavesItem::CopyChildren(FavesItemPtr source)
{
    for (const auto& child : source->m_children) {
        auto newItem = std::make_unique<FavesItem>(this, child->m_type, child->m_name, child->m_link);
        newItem->uParam = child->uParam;
        m_children.push_back(std::move(newItem));

        CopyChildren(child.get());
    }
}

VOID FavesItem::ClearChildren()
{
    m_children.clear();
}

VOID FavesItem::SortChildren()
{
    std::sort(m_children.begin(), m_children.end(), [](const auto &lhs, const auto &rhs) {
        if (lhs->IsGroup() && rhs->IsLink()) {
            return true;
        }
        if (lhs->IsLink() && rhs->IsGroup()) {
            return false;
        }
        return lhs->Name() < rhs->Name();
    });
}

VOID FavesItem::Remove()
{
    std::erase_if(m_parent->m_children, [this](const auto& item) {
        return item.get() == this;
    });
}

BOOL FavesItem::IsRoot() const
{
    return m_parent == nullptr;
}

BOOL FavesItem::IsGroup() const
{
    return m_link.empty();
}

BOOL FavesItem::IsLink() const
{
    return !m_link.empty();
}

BOOL FavesItem::HasChildren() const
{
    return !m_children.empty();
}

VOID FavesItem::AddChild(std::unique_ptr<FavesItem>&& child)
{
    m_children.push_back(std::move(child));
}
