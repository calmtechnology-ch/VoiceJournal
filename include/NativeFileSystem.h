/**
 * @file NativeFileSystem.h
 * @brief Implémentation IFileSystem pour tests natifs C++17 et simulateur PC — VoiceJournal v0.3.0
 */

#pragma once

#include "IFileSystem.h"
#include <cstdio>
#include <string>

namespace VoiceJournal {

class NativeFileHandle final : public IFileHandle {
public:
    explicit NativeFileHandle(FILE* fp) : fp_(fp) {}
    ~NativeFileHandle() override {
        if (fp_) std::fclose(fp_);
    }

    size_t read(uint8_t* buf, size_t size) override {
        if (!fp_) return 0;
        return std::fread(buf, 1, size, fp_);
    }

    size_t write(const uint8_t* buf, size_t size) override {
        if (!fp_) return 0;
        return std::fwrite(buf, 1, size, fp_);
    }

    bool seek(size_t pos) override {
        if (!fp_) return false;
        return std::fseek(fp_, static_cast<long>(pos), SEEK_SET) == 0;
    }

    size_t position() override {
        if (!fp_) return 0;
        long p = std::ftell(fp_);
        return (p >= 0) ? static_cast<size_t>(p) : 0;
    }

    size_t size() override {
        if (!fp_) return 0;
        long cur = std::ftell(fp_);
        std::fseek(fp_, 0, SEEK_END);
        long sz = std::ftell(fp_);
        std::fseek(fp_, cur, SEEK_SET);
        return (sz >= 0) ? static_cast<size_t>(sz) : 0;
    }

    void flush() override {
        if (fp_) std::fflush(fp_);
    }

    void close() override {
        if (fp_) {
            std::fclose(fp_);
            fp_ = nullptr;
        }
    }

    bool truncate(size_t newSize) override;

private:
    FILE* fp_ = nullptr;
};

class NativeFileSystem final : public IFileSystem {
public:
    explicit NativeFileSystem(const std::string& baseDir = ".");

    bool exists(const char* path) override;
    bool remove(const char* path) override;
    bool rename(const char* oldPath, const char* newPath) override;
    size_t fileSize(const char* path) override;
    IFileHandle* open(const char* path, const char* mode) override;
    size_t totalBytes() override { return 16 * 1024 * 1024; }
    size_t usedBytes() override;
    const std::string& getBaseDir() const { return baseDir_; }

private:
    std::string baseDir_;
    std::string resolvePath(const char* path) const;
};

} // namespace VoiceJournal
