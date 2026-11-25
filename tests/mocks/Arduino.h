#pragma once

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

// Horloge simulee controlable dans les tests natifs
inline uint32_t& mockMillisStorage() {
    static uint32_t t = 1000;
    return t;
}

inline uint32_t millis() {
    return mockMillisStorage();
}

inline void setMockMillis(uint32_t ms) {
    mockMillisStorage() = ms;
}

inline void advanceMockMillis(uint32_t deltaMs) {
    mockMillisStorage() += deltaMs;
}

// Emulation legere de la classe Arduino String pour tests natifs C++17
class String {
public:
    String() = default;
    String(const char* s) : data_(s ? s : "") {}
    String(const std::string& s) : data_(s) {}
    explicit String(unsigned int v) : data_(std::to_string(v)) {}
    explicit String(int v) : data_(std::to_string(v)) {}

    size_t length() const { return data_.size(); }
    bool isEmpty() const { return data_.empty(); }
    const char* c_str() const { return data_.c_str(); }
    void reserve(size_t n) { data_.reserve(n); }

    char operator[](size_t i) const { return data_[i]; }
    char& operator[](size_t i) { return data_[i]; }

    void trim() {
        size_t start = 0;
        while (start < data_.size() &&
               std::isspace(static_cast<unsigned char>(data_[start]))) {
            ++start;
        }
        size_t end = data_.size();
        while (end > start &&
               std::isspace(static_cast<unsigned char>(data_[end - 1]))) {
            --end;
        }
        data_ = data_.substr(start, end - start);
    }

    String substring(size_t beginIndex) const {
        if (beginIndex >= data_.size()) return String("");
        return String(data_.substr(beginIndex));
    }

    String substring(size_t beginIndex, size_t endIndex) const {
        if (beginIndex >= data_.size() || endIndex <= beginIndex) return String("");
        return String(data_.substr(beginIndex, endIndex - beginIndex));
    }

    int indexOf(const char* sub) const {
        if (!sub) return -1;
        size_t pos = data_.find(sub);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }

    bool startsWith(const char* prefix) const {
        return indexOf(prefix) == 0;
    }

    bool endsWith(const char* suffix) const {
        if (!suffix) return false;
        size_t len = std::strlen(suffix);
        if (data_.size() < len) return false;
        return data_.compare(data_.size() - len, len, suffix) == 0;
    }

    String& operator+=(const String& rhs) { data_ += rhs.data_; return *this; }
    String& operator+=(const char* rhs) { if (rhs) data_ += rhs; return *this; }
    String& operator+=(char c) { data_.push_back(c); return *this; }

    friend String operator+(const String& a, const String& b) { return String(a.data_ + b.data_); }
    friend String operator+(const String& a, const char* b) { return String(a.data_ + (b ? b : "")); }
    friend String operator+(const char* a, const String& b) { return String(std::string(a ? a : "") + b.data_); }

    bool operator==(const String& rhs) const { return data_ == rhs.data_; }
    bool operator==(const char* rhs) const { return data_ == (rhs ? rhs : ""); }
    bool operator!=(const String& rhs) const { return data_ != rhs.data_; }
    bool operator!=(const char* rhs) const { return data_ != (rhs ? rhs : ""); }

private:
    std::string data_;
};

// Mock Serial silencieux par defaut pour garder la sortie de test lisible
class MockSerialClass {
public:
    void begin(uint32_t) {}
    void print(const char* = "") {}
    void println(const char* = "") {}
    void printf(const char* fmt, ...) {
        (void)fmt;
    }
};

inline MockSerialClass Serial;
