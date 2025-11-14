#include "UTF16Stream.h"
#include <stdexcept>

namespace {
constexpr wchar_t UTF16LE_BOM = L'\xFEFF';

std::wstring ConvertUtf8ToUtf16(std::string_view utf8_str)
{
    if (utf8_str.empty()) {
        return {};
    }

    std::wstring result;
    size_t i = 0;

    while (i < utf8_str.length()) {
        uint32_t codepoint = 0;
        size_t bytes = 0;

        unsigned char c = static_cast<unsigned char>(utf8_str[i]);

        if (c < 0x80) {
            codepoint = c;
            bytes = 1;
        }
        else if ((c & 0xE0) == 0xC0) {
            codepoint = c & 0x1F;
            bytes = 2;
        }
        else if ((c & 0xF0) == 0xE0) {
            codepoint = c & 0x0F;
            bytes = 3;
        }
        else if ((c & 0xF8) == 0xF0) {
            codepoint = c & 0x07;
            bytes = 4;
        }
        else {
            // Invalid UTF-8 sequence - skip this byte
            ++i;
            continue;
        }

        // Process remaining bytes
        bool valid = true;
        for (size_t j = 1; j < bytes && (i + j) < utf8_str.length(); ++j) {
            unsigned char next = static_cast<unsigned char>(utf8_str[i + j]);
            if ((next & 0xC0) != 0x80) {
                // Invalid continuation byte
                valid = false;
                break;
            }
            codepoint = (codepoint << 6) | (next & 0x3F);
        }

        if (!valid || (i + bytes) > utf8_str.length()) {
            // Skip invalid sequence
            ++i;
            continue;
        }

        // Convert to UTF-16
        if (codepoint <= 0xFFFF) {
            // Character in BMP
            result += static_cast<wchar_t>(codepoint);
        }
        else if (codepoint <= 0x10FFFF) {
            // Surrogate pair required
            codepoint -= 0x10000;
            result += static_cast<wchar_t>(0xD800 + (codepoint >> 10));
            result += static_cast<wchar_t>(0xDC00 + (codepoint & 0x3FF));
        }

        i += bytes;
    }

    return result;
}

} // namespace


Utf16Reader::Utf16Reader(const std::filesystem::path& filename)
    : file_(filename, std::ios::binary)
{
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename.string());
    }

    wchar_t bom;
    ReadChar(bom);
    if (bom != UTF16LE_BOM) {
        file_.clear();
        file_.seekg(0, std::ios::beg);
    }
}

Utf16Reader::~Utf16Reader()
{
    close();
}

bool Utf16Reader::getline(std::wstring& line)
{
    line.clear();
    wchar_t ch;
    bool found_data = false;

    while (ReadChar(ch)) {
        found_data = true;
        if (ch == L'\n') {
            break;
        }
        if (ch == L'\r') {
            wchar_t next;
            if (ReadChar(next)) {
                if (next == L'\n') {
                    // Found \r\n - line ending
                    break;
                }
                // \r followed by something else - include only the next char
                line += next;
            }
            // Single \r as line ending
            break;
        }
        else {
            line += ch;
        }
    }
    return found_data;
}

bool Utf16Reader::eof() const
{
    return file_.eof();
}

void Utf16Reader::close() { file_.close(); }

bool Utf16Reader::ReadChar(wchar_t& ch)
{
    file_.read(reinterpret_cast<char*>(&ch), sizeof(ch));
    if (file_.gcount() != static_cast<std::streamsize>(sizeof(ch))) {
        file_.setstate(std::ios::eofbit);
        return false;
    }
    return true;
}



Utf16Writer::Utf16Writer(const std::filesystem::path& filename)
    : file_(filename, std::ios::binary)
{
    if (!file_.is_open())
    {
        throw std::runtime_error("Failed to open file: " + filename.string());
    }
    file_.write(reinterpret_cast<const char*>(&UTF16LE_BOM), sizeof(UTF16LE_BOM));
    if (!file_.good()) {
        throw std::runtime_error("Failed to write BOM to file: " + filename.string());
    }
}

Utf16Writer::~Utf16Writer()
{
    file_.close();
}

bool Utf16Writer::is_open() const
{
    return file_.is_open();
}

Utf16Writer& Utf16Writer::operator<<(std::wstring_view str)
{
    file_.write(reinterpret_cast<const char*>(str.data()),
                static_cast<std::streamsize>(str.length() * sizeof(wchar_t)));
    return *this;
}

Utf16Writer& Utf16Writer::operator<<(wchar_t ch)
{
    file_.write(reinterpret_cast<const char*>(&ch), sizeof(ch));
    return *this;
}

Utf16Writer& Utf16Writer::operator<<(const wchar_t* str)
{
    const std::wstring_view view(str);
    file_.write(reinterpret_cast<const char*>(view.data()),
                static_cast<std::streamsize>(view.length() * sizeof(wchar_t)));
    return *this;
}

Utf16Writer& Utf16Writer::operator<<(uint32_t value)
{
    const std::wstring str = std::to_wstring(value);
    file_.write(reinterpret_cast<const char*>(str.data()),
                static_cast<std::streamsize>(str.length() * sizeof(wchar_t)));
    return *this;
}

Utf16Writer& Utf16Writer::operator<<(std::string_view str)
{
    // Convert to UTF-16 from UTF-8
    std::wstring converted = ConvertUtf8ToUtf16(str);
    file_.write(reinterpret_cast<const char*>(converted.data()),
                static_cast<std::streamsize>(converted.length() * sizeof(wchar_t)));
    return *this;
}
