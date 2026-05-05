import re

with open('src/Explorer/ExplorerDialog.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

bad_block = r"""    // For now we will create a temporary entry and enqueue it, and handle it async.
    auto parentFolderPath = GetPath\(parentItem\);
    auto tempEntry = std::make_shared<ExplorerEntry>\(parentFolderPath, FileSystemEntry\(parentFolderPath, FILE_ATTRIBUTE_DIRECTORY, 0, 0, false\)\);
    _workerThread\.Enqueue\(std::make_unique<TaskUpdateDirectory>\(_model, tempEntry, _pSettings\)\);
\}

            else if \(_pSettings->IsUseFullTree\(\)\) \{
                files\.push_back\(entry\);
            \}
        \}

        /\* sort data \*/
        std::sort\(folders\.begin\(\), folders\.end\(\), \[\]\(const auto& lhs, const auto& rhs\) \{
            return ::StrCmpLogicalW\(lhs\.Name\(\)\.c_str\(\), rhs\.Name\(\)\.c_str\(\)\) < 0;
        \}\);
        std::sort\(files\.begin\(\), files\.end\(\), \[\]\(const auto& lhs, const auto& rhs\) \{
            return ::StrCmpLogicalW\(lhs\.Name\(\)\.c_str\(\), rhs\.Name\(\)\.c_str\(\)\) < 0;
        \}\);

        for \(const auto\* entries_ptr : \{ &folders, &files \}\) \{
            for \(const auto& entry : \*entries_ptr\) \{
                if \(InsertChildFolder\(entry\.Name\(\), parentItem\) == nullptr\) \{
                    break;
                \}
            \}
        \}
    \}"""

new_block = r"""    // For now we will create a temporary entry and enqueue it, and handle it async.
    auto parentFolderPath = GetPath(parentItem);
    auto tempEntry = std::make_shared<ExplorerEntry>(parentFolderPath, FileSystemEntry(parentFolderPath, FILE_ATTRIBUTE_DIRECTORY, 0, 0, false));
    _workerThread.Enqueue(std::make_unique<TaskUpdateDirectory>(_model, tempEntry, _pSettings));
}"""

content = re.sub(bad_block, new_block, content)

with open('src/Explorer/ExplorerDialog.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
