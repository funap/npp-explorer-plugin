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
{
    m_roots[static_cast<size_t>(FavesType::Folder)] = std::make_unique<FavesItem>(nullptr, FavesType::Folder, L"[Folders]");
    m_roots[static_cast<size_t>(FavesType::File)] = std::make_unique<FavesItem>(nullptr, FavesType::File, L"[Files]");
    m_roots[static_cast<size_t>(FavesType::Web)] = std::make_unique<FavesItem>(nullptr, FavesType::Web, L"[Web]");
    m_roots[static_cast<size_t>(FavesType::Session)] = std::make_unique<FavesItem>(nullptr, FavesType::Session, L"[Sessions]");
}

void FavesModel::Clear()
{
    for (auto& root : m_roots) {
        if (root) {
            root->ClearChildren();
        }
    }
}

FavesItem* FavesModel::FolderRoot() const
{
    return RootByType(FavesType::Folder);
}

FavesItem* FavesModel::FileRoot() const
{
    return RootByType(FavesType::File);
}

FavesItem* FavesModel::WebRoot() const
{
    return RootByType(FavesType::Web);
}

FavesItem* FavesModel::SessionRoot() const
{
    return RootByType(FavesType::Session);
}

FavesItem* FavesModel::RootByType(FavesType type) const
{
    return m_roots[static_cast<size_t>(type)].get();
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
    FavesItem* root{nullptr};
    std::stack<FavesItem*> parents;
    while (!file.eof()) {
        if (!file.getline(line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }

        bool matchesRoot = false;
        for (const auto& rootItem : m_roots) {
            if (line == rootItem->Name()) {
                root = rootItem.get();
                root->IsExpanded(ReadPropertyBool(file, PROPERTY_EXPAND));
                matchesRoot = true;
                break;
            }
        }

        if (matchesRoot) {
            continue;
        }

        FavesItem* const parent = parents.empty() ? root : parents.top();
        if (!parent) {
            continue;
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
            if (!parents.empty()) {
                parents.pop();
            }
        }
    }
}

void FavesModel::Save(const std::filesystem::path& path) const
{
    auto BoolFrom = [](bool value) -> std::wstring_view {
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

    for (const auto& root : m_roots) {
        if (root) {
            file << root->Name() << L"\n"
                 << L"Expand=" << BoolFrom(root->IsExpanded()) << L"\n\n";
            SaveItems(root.get(), file);
        }
    }
}


// ----------------------------------------------------------------------------
// FavesItem
// ----------------------------------------------------------------------------
FavesItem::FavesItem(FavesItem* parent, FavesType type)
    : m_parent(parent)
    , m_type(type)
{
}

FavesItem::FavesItem(FavesItem* parent, FavesType type, const std::wstring& name, const std::wstring& link)
    : m_parent(parent)
    , m_type(type)
    , m_name(name)
    , m_link(link)
{
}

FavesItem::FavesItem(FavesItem* parent, const FavesItem* other)
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

FavesItem* FavesItem::Root()
{
    if (IsRoot()) {
        return this;
    }
    return m_parent->Root();
}

FavesItem* FavesItem::Parent() const
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
        return false;
    }
    if (this == anotherNode) {
        return true;
    }
    return IsNodeDescendant(anotherNode->m_parent);
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
    if (m_parent) {
        std::erase_if(m_parent->m_children, [this](const auto& item) {
            return item.get() == this;
        });
    }
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
