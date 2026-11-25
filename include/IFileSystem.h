/**
 * @file IFileSystem.h
 * @brief Interface d'abstraction du système de fichiers pour LittleFS et tests natifs PC.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace VoiceJournal {

class IFileHandle {
public:
    virtual ~IFileHandle() = default;
    virtual size_t read(uint8_t* buf, size_t size) = 0;
    virtual size_t write(const uint8_t* buf, size_t size) = 0;
    virtual bool seek(size_t pos) = 0;
    virtual size_t position() = 0;
    virtual size_t size() = 0;
    virtual void flush() = 0;
    virtual void close() = 0;
    virtual bool truncate(size_t newSize) = 0;
};

class IFileSystem {
public:
    virtual ~IFileSystem() = default;
    virtual bool exists(const char* path) = 0;
    virtual bool remove(const char* path) = 0;
    virtual bool rename(const char* oldPath, const char* newPath) = 0;
    virtual size_t fileSize(const char* path) = 0;
    virtual IFileHandle* open(const char* path, const char* mode) = 0;
    virtual size_t totalBytes() = 0;
    virtual size_t usedBytes() = 0;
};

} // namespace VoiceJournal
