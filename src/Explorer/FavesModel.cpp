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

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <format>
#include <stack>

#include "UTF16Stream.h"

namespace {
constexpr std::wstring_view LINK_TAG        = L"#LINK";
constexpr std::wstring_view GROUP_TAG       = L"#GROUP";
constexpr std::wstring_view END_TAG         = L"#END";
constexpr std::wstring_view PROPERTY_NAME   = L"Name=";
constexpr std::wstring_view PROPERTY_LINK   = L"Link=";
constexpr std::wstring_view PROPERTY_EXPAND = L"Expand=";
}
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

void FavesModel::Load(const std::filesystem::path &path) {

    auto ReadPropertyString = [](Utf16Reader& file, std::wstring_view property) -> std::wstring {
        if (file.eof()) {
            return {};
        }
        std::wstring line;
        file.getline(line);
        size_t pos = line.find_first_of(property);
        if (std::wstring::npos == pos) {
            throw std::runtime_error("Invalid format: expected prefix not found");
        }
        return line.substr(pos + property.length());
    };

    auto ReadPropertyBool = [&](Utf16Reader& file, std::wstring_view property) -> bool {
        const auto value = ReadPropertyString(file, property);
        return value == L"1";
    };

    Clear();
    Utf16Reader file(path);
    std::wstring line;
    FavesItemPtr root{nullptr};
    std::stack<FavesItemPtr> parents;
    while (!file.eof()) {
        if (!file.getline(line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }
        if (line == FolderRoot()->Name()) {
            root = FolderRoot();
            root->IsExpanded(ReadPropertyBool(file, PROPERTY_EXPAND));
        }
        else if (line == FileRoot()->Name()) {
            root = FileRoot();
            root->IsExpanded(ReadPropertyBool(file, PROPERTY_EXPAND));
        }
        else if (line == WebRoot()->Name()) {
            root = WebRoot();
            root->IsExpanded(ReadPropertyBool(file, PROPERTY_EXPAND));
        }
        else if (line == SessionRoot()->Name()) {
            root = SessionRoot();
            root->IsExpanded(ReadPropertyBool(file, PROPERTY_EXPAND));
        }

        const FavesItemPtr parent = parents.empty() ? root :parents.top();
        if (!parent) {
            break;
        }
        if (line == LINK_TAG) {
            auto link = std::make_unique<FavesItem>(parent, parent->Root()->Type());
            link->Name(ReadPropertyString(file, PROPERTY_NAME));
            link->Link(ReadPropertyString(file, PROPERTY_LINK));
            parent->AddChild(std::move(link));
        } else if (line == GROUP_TAG) {
            auto group = std::make_unique<FavesItem>(parent, parent->Root()->Type());
            group->Name(ReadPropertyString(file, PROPERTY_NAME));
            group->IsExpanded(ReadPropertyBool(file, PROPERTY_EXPAND));
            parents.push(group.get());
            parent->AddChild(std::move(group));
        } else if (line == END_TAG) {
            parents.pop();
        }
    }
}

void FavesModel::Save(const std::filesystem::path& path) const
{
    auto BoolFrom = [](bool value) -> std::wstring {
        return value ? L"1" : L"0";
    };
    
    std::function<void(const FavesItem*, Utf16Writer&)> SaveItems = [&](const FavesItem* parent_item, Utf16Writer& file) -> void {
        for (const auto& item : parent_item->Children()) {
            if (item->IsGroup()) {
                file << GROUP_TAG << L"\n"
                     << L"\t" << PROPERTY_NAME << item->Name() << L"\n"
                     << L"\t" << PROPERTY_EXPAND << BoolFrom(item->IsExpanded()) << L"\n\n";
                SaveItems(item.get(), file);
                file << END_TAG << L"\n\n";
            }
            else if (item->IsLink()) {
                file << LINK_TAG << L"\n"
                     << L"\t" << PROPERTY_NAME << item->Name() << L"\n"
                     << L"\t" << PROPERTY_LINK << item->Link() << L"\n\n";
            }
        }
    };

    Utf16Writer file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + path.string());
    }

    for (const auto* root : {FolderRoot(), FileRoot(), WebRoot(), SessionRoot()}) {
        file << root->Name() << L"\n"
             << L"Expand=" << BoolFrom(root->IsExpanded()) << L"\n\n";
        SaveItems(root, file);
    }
}


// ----------------------------------------------------------------------------
// FavesItem
// ----------------------------------------------------------------------------
FavesItem::FavesItem(const FavesItemPtr parent, FavesType type)
    : m_parent(parent)
    , m_type(type)
{
}

FavesItem::FavesItem(const FavesItemPtr parent, FavesType type, const std::wstring& name, const std::wstring& link)
    : m_parent(parent)
    , m_type(type)
    , m_name(name)
    , m_link(link)
{
}

FavesItem::FavesItem(const FavesItemPtr parent, const FavesItemPtr other)
    : m_parent(parent)
    , m_type(other->m_type)
    , m_name(other->m_name)
    , m_link(other->m_link)
    , m_data(other->m_data)
{
    for (const auto& child : other->m_children) {
        auto newItem = std::make_unique<FavesItem>(this, child.get());
        m_children.push_back(std::move(newItem));
    }
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

FavesItemPtr FavesItem::Parent() const
{
    return m_parent;
}

FavesType FavesItem::Type() const
{
    return m_type;
}

const std::wstring& FavesItem::Name() const
{
    return m_name;
}

void FavesItem::Name(const std::wstring& name)
{
    m_name = name;
}

const std::wstring& FavesItem::Link() const
{
    return m_link;
}

void FavesItem::Link(const std::wstring& link)
{
    m_link = link;
}

bool FavesItem::IsExpanded() const
{
    return m_isExpanded;
}

void FavesItem::IsExpanded(bool isExpanded)
{
    m_isExpanded = isExpanded;
}

bool FavesItem::IsNodeDescendant(const FavesItem* anotherNode) const
{
    if (nullptr == anotherNode) {
        return FALSE;
    }
    if (this == anotherNode) {
        return TRUE;
    }
    return IsNodeDescendant(anotherNode->m_parent);
}

void FavesItem::CopyChildren(FavesItemPtr source)
{
    for (const auto& child : source->m_children) {
        auto newItem = std::make_unique<FavesItem>(this, child->m_type, child->m_name, child->m_link);
        newItem->m_data = child->m_data;
        m_children.push_back(std::move(newItem));

        CopyChildren(child.get());
    }
}

void FavesItem::ClearChildren()
{
    m_children.clear();
}

void FavesItem::SortChildren()
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

void FavesItem::Remove()
{
    std::erase_if(m_parent->m_children, [this](const auto& item) {
        return item.get() == this;
    });
}

bool FavesItem::IsRoot() const
{
    return m_parent == nullptr;
}

bool FavesItem::IsGroup() const
{
    return m_link.empty();
}

bool FavesItem::IsLink() const
{
    return !m_link.empty();
}

bool FavesItem::HasChildren() const
{
    return !m_children.empty();
}

void FavesItem::AddChild(std::unique_ptr<FavesItem>&& child)
{
    m_children.push_back(std::move(child));
}

const std::vector<std::unique_ptr<FavesItem>>& FavesItem::Children() const { return m_children; }

uint32_t FavesItem::Data() const
{
    return m_data;
}
void FavesItem::Data(uint32_t data)
{
    m_data = data;
}
