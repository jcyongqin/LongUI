﻿#pragma once
/**
* Copyright (c) 2014-2016 dustpg   mailto:dustpg@gmail.com
*
* Permission is hereby granted, free of charge, to any person
* obtaining a copy of this software and associated documentation
* files (the "Software"), to deal in the Software without
* restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following
* conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
* HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "../luiconf.h"
#include <cstdint>
#include <cassert>

// LongUI namespace
namespace LongUI {
    // UI String -- compatible with std library string interface(part of) but host a fixed buffer
    class CUIString {
        // my version strlen
        static inline auto uistrlen(const char* str) noexcept { return static_cast<uint32_t>(std::strlen(str)); }
        // my version wcslen
        static inline auto uistrlen(const wchar_t* str) noexcept { return static_cast<uint32_t>(std::wcslen(str)); }
    public:
        // ctor
        CUIString() noexcept { *m_aDataStatic = 0; }
        // ctor with data+len
        CUIString(const wchar_t* str, uint32_t len) noexcept { this->Set(str, len); }
        // ctor with data
        CUIString(const wchar_t* str) noexcept { this->Set(str); }
        // dtor
        ~CUIString() noexcept;
        // copy ctor, will set performance warning
        CUIString(const CUIString&) noexcept;
        // move ctor, will set performance warning
        CUIString(CUIString&&) noexcept;
    public: // LongUI Style
        // make from utf8
        void FromUtf8(const char* str = nullptr) noexcept;
        // set/assign
        void Set(const wchar_t* str, uint32_t len) noexcept;
        // set/assign inline overload
        void Set(const wchar_t* str) noexcept { return this->Set(str, this->uistrlen(str)); }
        // append
        void Append(const wchar_t* str, uint32_t len) noexcept;
        // set/assign for utf-8 inline overload
        void Append(const wchar_t* str) noexcept { return this->Append(str, this->uistrlen(str)); }
        // reserve
        void Reserve(uint32_t len) noexcept;
        // insert
        void Insert(uint32_t offset, const wchar_t* str, uint32_t len) noexcept;
        // insert for inline overload
        void Insert(uint32_t offset, const wchar_t* str) noexcept { return this->Insert(offset, str, this->uistrlen(str)); }
        // remove
        void Remove(uint32_t offset, uint32_t length) noexcept;
        // format
        void Format(const wchar_t* format, ...) noexcept;
        // On Out of Memory
        void OnOOM() noexcept;
    public: // std::string compatibled interface/method
        // begin
        auto begin() noexcept { return m_pString; }
        // end
        auto end() noexcept { return m_pString + m_cLength; }
        // begin - const
        const auto* begin() const noexcept { return m_pString; }
        // end - const
        const auto* end() const noexcept { return m_pString + m_cLength; }
        // is empty?
        auto empty() const noexcept { return !m_cLength; }
        // reserve
        auto reserve(uint32_t len) noexcept { return this->Reserve(len); }
        // get data overload for const
        auto*data() noexcept { return m_pString; }
        // get data length
        auto length() const noexcept { return m_cLength; }
        // get size
        auto size() const noexcept { return m_cLength; }
        // get data
        const auto* data() const noexcept { return m_pString; }
        // c style string
        const auto* c_str() const noexcept { return m_pString; }
    public: // std::string compatibled interface/method
        // insert for string
        auto&insert(uint32_t off, const wchar_t* str, uint32_t len) noexcept { this->Insert(off, str, len); return *this; }
        // insert for string
        auto&insert(uint32_t off, const wchar_t* str) noexcept { this->Insert(off, str); return *this; }
        // insert for wchar
        auto&insert(uint32_t off, const wchar_t ch) noexcept { wchar_t str[2] = { ch, 0 }; return this->insert(off, str, 1); }
        // insert for CUIString
        auto&insert(uint32_t off, const CUIString& str) noexcept { return this->insert(off, str.c_str(), str.length()); }
        // erase
        auto&erase(uint32_t off, uint32_t len) noexcept { this->Remove(off, len); return *this; }
        // at
        auto at(uint32_t off) const noexcept { assert(off < m_cLength && "out of range"); return m_pString[off]; }
        // [] operator
        auto&operator[](uint32_t off) noexcept { assert(off < m_cLength && "out of range"); return m_pString[off]; }
        // [] for const
        const auto&operator[](uint32_t off) const noexcept { assert(off < m_cLength && "out of range"); return m_pString[off]; }
    public: // std::string compatibled interface/method
        // assign: overload for [const wchar_t* + len]
        auto&assign(const wchar_t* str, uint32_t count) noexcept { this->Set(str, count); return *this; }
        // assign: overload for [const wchar_t* + len]
        auto&assign(const wchar_t* str, const wchar_t* end) noexcept { this->Set(str, uint32_t(end-str)); return *this; }
        // assign: overload for [const wchar_t*]
        auto&assign(const wchar_t* str) noexcept { this->Set(str); return *this; }
        // assign: overload for [const wchar_t]
        auto&assign(const wchar_t ch) noexcept { wchar_t buf[2] = { ch, 0 }; this->Set(buf, 1); return *this; }
        // = 操作: overload for [const wchar_t*]
        auto&operator= (const wchar_t* s) noexcept { this->assign(s); return *this; }
        // = 操作: overload for [const wchar_t]
        auto&operator= (const wchar_t ch) noexcept { this->assign(ch); return *this; }
        // = 操作: overload for [CUIString]
        auto&operator= (const CUIString& str) noexcept { this->assign(str.c_str(), str.length()); return *this; }
    public: // std::string compatibled interface/method
        // append overload for [const wchar_t*]
        auto&append(const wchar_t* str, uint32_t count) noexcept { this->Append(str, count); return *this; }
        // append overload for [const wchar_t*]
        auto&append(const wchar_t* str) noexcept { this->Append(str); return *this; }
        // append overload for [const wchar_t]
        auto&append(const wchar_t ch) noexcept { wchar_t buf[2] = { ch, 0 }; this->Append(buf, 1); return *this; }
        // append overload for [const CUIString&]
        auto&append(const CUIString& str) noexcept { this->append(str.c_str(), str.length()); return *this; }
        // += 操作 overload for [const wchar_t*]
        auto&operator+=(const wchar_t* s) noexcept { this->append(s); return *this; }
        // += 操作 overload for [const wchar_t]
        auto&operator+=(const wchar_t c) noexcept { this->append(c); return *this; }
        // += 操作 overload for [longui string]
        auto&operator+=(const CUIString& str) noexcept { this->append(str.c_str(), str.length()); return *this; }
    public:
        // == 操作
        bool operator==(const wchar_t* str) const noexcept { return (std::wcscmp(m_pString, str) == 0); };
        // != 操作
        bool operator!=(const wchar_t* str) const noexcept { return (std::wcscmp(m_pString, str) != 0); };
        // <= 操作
        bool operator<=(const wchar_t* str) const noexcept { return (std::wcscmp(m_pString, str) <= 0); };
        // < 操作
        bool operator< (const wchar_t* str) const noexcept { return (std::wcscmp(m_pString, str) < 0); };
        // >= 操作
        bool operator>=(const wchar_t* str) const noexcept { return (std::wcscmp(m_pString, str) >= 0); };
        // > 操作
        bool operator> (const wchar_t* str) const noexcept { return (std::wcscmp(m_pString, str) > 0); };
        // == 操作 for CUIString
        bool operator==(const CUIString& str) const noexcept { return (std::wcscmp(m_pString, str.m_pString) == 0); };
        // != 操作 for CUIString
        bool operator!=(const CUIString& str) const noexcept { return (std::wcscmp(m_pString, str.m_pString) != 0); };
        // <= 操作 for CUIString
        bool operator<=(const CUIString& str) const noexcept { return (std::wcscmp(m_pString, str.m_pString) <= 0); };
        // < 操作 for CUIString
        bool operator< (const CUIString& str) const noexcept { return (std::wcscmp(m_pString, str.m_pString) < 0); };
        // >= 操作 for CUIString
        bool operator>=(const CUIString& str) const noexcept { return (std::wcscmp(m_pString, str.m_pString) >= 0); };
        // > 操作 for CUIString
        bool operator> (const CUIString& str) const noexcept { return (std::wcscmp(m_pString, str.m_pString) > 0); };
        // 转换为 wchar_t*
        operator const wchar_t*()const noexcept { return m_pString; }
    private:
        // 字符串
        wchar_t*            m_pString = m_aDataStatic;
        // 字符串长度
        uint32_t            m_cLength = 0;
        // 缓冲区长度
        uint32_t            m_cBufferLength = LongUIStringFixedLength;
        // 静态数据
        wchar_t             m_aDataStatic[LongUIStringFixedLength];
    private:
        // alloc buffer in safe way
        static inline auto alloc_bufer(uint32_t buffer_length) {
            size_t length = sizeof(wchar_t) * size_t(buffer_length);
            return reinterpret_cast<wchar_t*>(LongUI::SmallAlloc(length));
        }
        // copy string
        static inline auto copy_string(wchar_t* __restrict des, const wchar_t* __restrict src, uint32_t length) {
            std::memcpy(des, src, sizeof(wchar_t) * (length + 1));
        }
        // copy string without null-end char
        static inline auto copy_string_ex(wchar_t* __restrict des, const wchar_t* __restrict src, uint32_t length) {
            std::memcpy(des, src, sizeof(wchar_t) * (length));
        }
        // choose a nice length for buffer
        static inline auto nice_buffer_length(uint32_t target) {
            auto sizex2 = sizeof(void*) * 2;
            auto temp = target + (target + LongUIStringFixedLength) / 2 + sizex2;
            return temp - temp % sizex2;
        }
        // free buffer in safe way
        auto inline safe_free_bufer() {
            if (m_pString && m_pString != m_aDataStatic) {
                LongUI::SmallFree(m_pString);
            }
            m_pString = nullptr;
        }
    };
    // Wrapped const char*
    class CUIWrappedCCP {
    public:
        // length
        auto length() const noexcept { return std::strlen(m_pString); }
        // c-string
        auto c_str() const noexcept { return m_pString; }
        // empty
        bool empty() const noexcept { return !m_pString[0]; }
        // [] for const
        auto&operator[](size_t i) const noexcept { return m_pString[i]; }
        // = 
        auto&operator=(const char* str) noexcept { m_pString = str; return *this; }
    public:
        // ctor
        CUIWrappedCCP(const char* str="") noexcept : m_pString(str) {}
        // == 操作
        bool operator==(const char* str) const noexcept { return (std::strcmp(m_pString, str) == 0); };
        // != 操作
        bool operator!=(const char* str) const noexcept { return (std::strcmp(m_pString, str) != 0); };
        // <= 操作
        bool operator<=(const char* str) const noexcept { return (std::strcmp(m_pString, str) <= 0); };
        // < 操作
        bool operator< (const char* str) const noexcept { return (std::strcmp(m_pString, str) < 0); };
        // >= 操作
        bool operator>=(const char* str) const noexcept { return (std::strcmp(m_pString, str) >= 0); };
        // > 操作
        bool operator> (const char* str) const noexcept { return (std::strcmp(m_pString, str) > 0); };
        // == 操作 for CUIWrappedCCP
        bool operator==(const CUIWrappedCCP& str) const noexcept { return (std::strcmp(m_pString, str.m_pString) == 0); };
        // != 操作 for CUIWrappedCCP
        bool operator!=(const CUIWrappedCCP& str) const noexcept { return (std::strcmp(m_pString, str.m_pString) != 0); };
        // <= 操作 for CUIWrappedCCP
        bool operator<=(const CUIWrappedCCP& str) const noexcept { return (std::strcmp(m_pString, str.m_pString) <= 0); };
        // < 操作 for CUIWrappedCCP
        bool operator< (const CUIWrappedCCP& str) const noexcept { return (std::strcmp(m_pString, str.m_pString) < 0); };
        // >= 操作 for CUIWrappedCCP
        bool operator>=(const CUIWrappedCCP& str) const noexcept { return (std::strcmp(m_pString, str.m_pString) >= 0); };
        // > 操作 for CUIWrappedCCP
        bool operator> (const CUIWrappedCCP& str) const noexcept { return (std::strcmp(m_pString, str.m_pString) > 0); };
    private:
        // string data
        const char*         m_pString = nullptr;
    };
}