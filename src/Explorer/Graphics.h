// The MIT License (MIT)
//
// Copyright (c) 2023 funap
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
