#pragma once

#include <Windows.h>

#include <utility>

class Brush {
public:
    Brush() : m_hBrush(nullptr) {}
    explicit Brush(HBRUSH hBrush) : m_hBrush(hBrush) {}
    ~Brush() { Reset(); }
    Brush(const Brush&) = delete;
    Brush& operator=(const Brush&) = delete;
    Brush(Brush&& other) noexcept : m_hBrush(nullptr) {
        std::swap(m_hBrush, other.m_hBrush);
    }
    Brush& operator=(Brush&& other) noexcept {
        std::swap(m_hBrush, other.m_hBrush);
        return *this;
    }

    operator HBRUSH() const {
        return m_hBrush;
    }

    bool CreateSolidBrush(COLORREF color) {
        Reset();
        m_hBrush = ::CreateSolidBrush(color);
        return (m_hBrush != nullptr);
    }

    void Reset() {
        if (m_hBrush != nullptr) {
            ::DeleteObject(m_hBrush);
            m_hBrush = nullptr;
        }
    }

    HBRUSH Detach() {
        HBRUSH hBrush = m_hBrush;
        m_hBrush = nullptr;
        return hBrush;
    }

    void Attach(HBRUSH hBrush) {
        Reset();
        m_hBrush = hBrush;
    }

private:
    HBRUSH m_hBrush;
};
