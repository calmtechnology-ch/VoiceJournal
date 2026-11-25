/**
 * @file NativeFileSystem.cpp
 * @brief Implémentation du système de fichiers natif C++17 — VoiceJournal v0.3.0
 */

#include "NativeFileSystem.h"
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#if defined(_WIN32)
#include <io.h>
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#else
#include <unistd.h>
#define MKDIR(d) mkdir(d, 0755)
#endif

namespace VoiceJournal {

bool NativeFileHandle::truncate(size_t newSize) {
    if (!fp_) return false;
    std::fflush(fp_);
#if defined(_WIN32)
    int fd = _fileno(fp_);
    return _chsize(fd, static_cast<long>(newSize)) == 0;
#else
    int fd = fileno(fp_);
    return ftruncate(fd, static_cast<off_t>(newSize)) == 0;
#endif
}

NativeFileSystem::NativeFileSystem(const std::string& baseDir)
    : baseDir_(baseDir) {
    // Normaliser baseDir
    if (!baseDir_.empty() && (baseDir_.back() == '/' || baseDir_.back() == '\\')) {
        baseDir_.pop_back();
    }
    if (!baseDir_.empty()) {
        MKDIR(baseDir_.c_str());
    }
}

std::string NativeFileSystem::resolvePath(const char* path) const {
    if (!path) return baseDir_;
    std::string p(path);
    if (!p.empty() && (p.front() == '/' || p.front() == '\\')) {
        p = p.substr(1);
    }
    if (baseDir_.empty()) return p;
    return baseDir_ + "/" + p;
}

bool NativeFileSystem::exists(const char* path) {
    std::string fullPath = resolvePath(path);
    struct stat st;
    return stat(fullPath.c_str(), &st) == 0;
}

bool NativeFileSystem::remove(const char* path) {
    std::string fullPath = resolvePath(path);
    return std::remove(fullPath.c_str()) == 0;
}

bool NativeFileSystem::rename(const char* oldPath, const char* newPath) {
    std::string fullOld = resolvePath(oldPath);
    std::string fullNew = resolvePath(newPath);
    // Sous Windows, std::rename échoue si le fichier cible existe déjà : le supprimer d'abord
    std::remove(fullNew.c_str());
    return std::rename(fullOld.c_str(), fullNew.c_str()) == 0;
}

size_t NativeFileSystem::fileSize(const char* path) {
    std::string fullPath = resolvePath(path);
    struct stat st;
    if (stat(fullPath.c_str(), &st) == 0) {
        return static_cast<size_t>(st.st_size);
    }
    return 0;
}

IFileHandle* NativeFileSystem::open(const char* path, const char* mode) {
    std::string fullPath = resolvePath(path);
    std::string openMode(mode);
    // Assurer le mode binaire
    if (openMode.find('b') == std::string::npos) {
        openMode += "b";
    }

    FILE* fp = std::fopen(fullPath.c_str(), openMode.c_str());
    if (!fp) return nullptr;
    return new NativeFileHandle(fp);
}

size_t NativeFileSystem::usedBytes() {
    return fileSize("/journal.log");
}

} // namespace VoiceJournal
