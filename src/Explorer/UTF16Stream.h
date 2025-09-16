#pragma once
#include <filesystem>
#include <fstream>

class Utf16Reader {
public:
    explicit Utf16Reader(const std::filesystem::path& filename);
    ~Utf16Reader();

    bool getline(std::wstring& line);
    bool eof() const;
    void close();

private:
    bool ReadChar(wchar_t& ch);
    std::ifstream file_;
};

class Utf16Writer
{
public:
    explicit Utf16Writer(const std::filesystem::path& filename);
    ~Utf16Writer();

    bool is_open() const;

    Utf16Writer& operator<<(const std::wstring& str);
    Utf16Writer& operator<<(std::wstring_view str);
    Utf16Writer& operator<<(std::string_view str);
    Utf16Writer& operator<<(const wchar_t* str);
    Utf16Writer& operator<<(wchar_t ch);
    Utf16Writer& operator<<(uint32_t value);

private:
    std::ofstream file_;
};
