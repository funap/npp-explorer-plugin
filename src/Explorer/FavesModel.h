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
#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <memory>

enum FavesType {
    FAVES_FOLDER = 0,
    FAVES_FILE,
    FAVES_WEB,
    FAVES_SESSION,
    FAVES_TYPE_MAX,
};

constexpr UINT FAVES_PARAM_USERIMAGE = 0x00000200;

class FavesItem;
using FavesItemPtr = FavesItem*;

class FavesModel
{
public:
    FavesModel();
    ~FavesModel();
    void Clear();
    FavesItemPtr FolderRoot() const;
    FavesItemPtr FileRoot() const;
    FavesItemPtr WebRoot() const;
    FavesItemPtr SessionRoot() const;

private:
    std::unique_ptr<FavesItem> m_folders;
    std::unique_ptr<FavesItem> m_files;
    std::unique_ptr<FavesItem> m_webs;
    std::unique_ptr<FavesItem> m_sessions;
};

class FavesItem {
public:
    FavesItem() = delete;
    FavesItem(const FavesItemPtr parent, FavesType type, const std::wstring& name, const std::wstring& link = L"");
    ~FavesItem();

    FavesItemPtr        Root();
    FavesType           Type() const;
    const std::wstring& Name() const;
    const std::wstring& Link() const;
    BOOL                IsExpanded() const;
    VOID                IsExpanded(BOOL isExpanded);

    BOOL IsNodeDescendant(const FavesItem* anotherNode) const;
    VOID CopyChildren(FavesItemPtr source);
    VOID ClearChildren();
    VOID SortChildren();
    VOID Remove();
    BOOL IsRoot() const;
    BOOL IsGroup() const;
    BOOL IsLink() const;
    BOOL HasChildren() const;
    VOID AddChild(std::unique_ptr<FavesItem>&& child);

    const FavesItemPtr                      m_parent;
    const FavesType                         m_type;
    UINT                                    uParam{};
    std::wstring                            m_name;
    std::wstring                            m_link;
    BOOL                                    m_isExpanded;
    std::vector<std::unique_ptr<FavesItem>> m_children;
};
