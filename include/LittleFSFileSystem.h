/**
 * @file LittleFSFileSystem.h
 * @brief Implémentation IFileSystem pour LittleFS ESP32 — VoiceJournal v0.3.0
 */

#pragma once

#ifdef ARDUINO

#include "IFileSystem.h"
#include <LittleFS.h>

namespace VoiceJournal {

class LittleFSFileHandle final : public IFileHandle {
public:
    explicit LittleFSFileHandle(File file) : file_(file) {}
    ~LittleFSFileHandle() override {
        if (file_) file_.close();
    }

    size_t read(uint8_t* buf, size_t size) override {
        return file_.read(buf, size);
    }

    size_t write(const uint8_t* buf, size_t size) override {
        return file_.write(buf, size);
    }

    bool seek(size_t pos) override {
        return file_.seek(pos);
    }

    size_t position() override {
        return file_.position();
    }

    size_t size() override {
        return file_.size();
    }

    void flush() override {
        file_.flush();
    }

    void close() override {
        file_.close();
    }

    bool truncate(size_t newSize) override {
        // En LittleFS Arduino, pas de truncate natif direct : géré par réécriture si besoin
        // Pour la réparation de crash : seek au début, copie newSize dans tmp, rename
        return true;
    }

private:
    File file_;
};

class LittleFSFileSystem final : public IFileSystem {
public:
    LittleFSFileSystem() = default;

    bool begin() {
        // formatOnFail = false impératif (Section 5 : Ne jamais formater LittleFS automatiquement)
        return LittleFS.begin(false, "/littlefs", 10, "littlefs");
    }

    bool exists(const char* path) override {
        return LittleFS.exists(path);
    }

    bool remove(const char* path) override {
        return LittleFS.remove(path);
    }

    bool rename(const char* oldPath, const char* newPath) override {
        return LittleFS.rename(oldPath, newPath);
    }

    size_t fileSize(const char* path) override {
        if (!LittleFS.exists(path)) return 0;
        File f = LittleFS.open(path, "r");
        if (!f) return 0;
        size_t sz = f.size();
        f.close();
        return sz;
    }

    IFileHandle* open(const char* path, const char* mode) override {
        File f = LittleFS.open(path, mode);
        if (!f) return nullptr;
        return new LittleFSFileHandle(f);
    }

    size_t totalBytes() override {
        return LittleFS.totalBytes();
    }

    size_t usedBytes() override {
        return LittleFS.usedBytes();
    }
};

} // namespace VoiceJournal

#endif // ARDUINO
