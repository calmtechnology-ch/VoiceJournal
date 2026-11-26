/**
 * @file LittleFSFileSystem.h
 * @brief Implémentation IFileSystem pour LittleFS ESP32 — VoiceJournal v0.3.0
 */

#pragma once

#ifdef ARDUINO

#include "IFileSystem.h"
#include <LittleFS.h>
#include <string>

namespace VoiceJournal {

class LittleFSFileHandle final : public IFileHandle {
public:
    LittleFSFileHandle(File file, std::string path, std::string mode)
        : file_(file), path_(std::move(path)), mode_(std::move(mode)) {}
    ~LittleFSFileHandle() override {
        if (file_) file_.close();
    }

    size_t read(uint8_t* buf, size_t size) override {
        if (!file_) return 0;
        return file_.read(buf, size);
    }

    size_t write(const uint8_t* buf, size_t size) override {
        if (!file_) return 0;
        return file_.write(buf, size);
    }

    bool seek(size_t pos) override {
        if (!file_) return false;
        return file_.seek(pos);
    }

    size_t position() override {
        if (!file_) return 0;
        return file_.position();
    }

    size_t size() override {
        if (!file_) return 0;
        return file_.size();
    }

    void flush() override {
        if (file_) file_.flush();
    }

    void close() override {
        if (file_) file_.close();
    }

    bool truncate(size_t newSize) override {
        if (!file_ || path_.empty()) {
            return false;
        }
        file_.flush();
        const size_t currentSize = file_.size();
        if (newSize > currentSize) {
            return false;
        }
        if (newSize == currentSize) {
            return true;
        }

        const std::string tmpPath = path_ + ".trunc";
        const std::string bakPath = path_ + ".bak";

        if (LittleFS.exists(tmpPath.c_str())) {
            LittleFS.remove(tmpPath.c_str());
        }

        File tmp = LittleFS.open(tmpPath.c_str(), "w");
        if (!tmp) {
            return false;
        }

        if (!file_.seek(0)) {
            tmp.close();
            LittleFS.remove(tmpPath.c_str());
            return false;
        }

        uint8_t buf[256];
        size_t remaining = newSize;
        size_t totalCopied = 0;
        while (remaining > 0) {
            const size_t chunk = (remaining < sizeof(buf)) ? remaining : sizeof(buf);
            const size_t rd = file_.read(buf, chunk);
            if (rd != chunk) {
                tmp.close();
                LittleFS.remove(tmpPath.c_str());
                return false;
            }
            const size_t wr = tmp.write(buf, rd);
            if (wr != rd) {
                tmp.close();
                LittleFS.remove(tmpPath.c_str());
                return false;
            }
            remaining -= rd;
            totalCopied += wr;
        }

        tmp.flush();
        const size_t tmpSize = tmp.size();
        tmp.close();

        if (totalCopied != newSize || tmpSize != newSize) {
            LittleFS.remove(tmpPath.c_str());
            return false;
        }

        // Vérifier que le fichier temporaire est bien lisible et de la taille attendue
        File verify = LittleFS.open(tmpPath.c_str(), "r");
        if (!verify || verify.size() != newSize) {
            if (verify) verify.close();
            LittleFS.remove(tmpPath.c_str());
            return false;
        }
        verify.close();

        // Fermer le fichier original avant remplacement sécurisé
        file_.close();

        if (LittleFS.exists(bakPath.c_str())) {
            LittleFS.remove(bakPath.c_str());
        }
        if (!LittleFS.rename(path_.c_str(), bakPath.c_str())) {
            LittleFS.remove(tmpPath.c_str());
            file_ = LittleFS.open(path_.c_str(), mode_.c_str());
            return false;
        }

        if (!LittleFS.rename(tmpPath.c_str(), path_.c_str())) {
            // Restaurer le fichier original depuis la sauvegarde
            LittleFS.rename(bakPath.c_str(), path_.c_str());
            LittleFS.remove(tmpPath.c_str());
            file_ = LittleFS.open(path_.c_str(), mode_.c_str());
            return false;
        }

        LittleFS.remove(bakPath.c_str());
        file_ = LittleFS.open(path_.c_str(), mode_.c_str());
        if (file_) {
            file_.seek(newSize);
        }
        return static_cast<bool>(file_);
    }

private:
    File file_;
    std::string path_;
    std::string mode_;
};

class LittleFSFileSystem final : public IFileSystem {
public:
    LittleFSFileSystem() = default;

    bool begin() {
        // formatOnFail = false impératif (COR-ST05 : ne jamais formater LittleFS automatiquement)
        mounted_ = LittleFS.begin(false, "/littlefs", 10, "littlefs");
        if (!mounted_) {
            lastError_ = "LITTLEFS_MOUNT_FAILED";
        } else {
            lastError_.clear();
        }
        return mounted_;
    }

    /**
     * @brief Procédure explicite et sécurisée de première initialisation (jamais appelée automatiquement).
     *        Refuse de formater si la partition est déjà montée et contient des données.
     */
    bool formatExplicitly() {
        if (mounted_ && LittleFS.exists("/journal.log")) {
            lastError_ = "FORMAT_REFUSED_DATA_EXISTS";
            return false;
        }
        if (mounted_) {
            LittleFS.end();
            mounted_ = false;
        }
        if (!LittleFS.format()) {
            lastError_ = "LITTLEFS_FORMAT_FAILED";
            return false;
        }
        return begin();
    }

    bool isMounted() const override {
        return mounted_;
    }

    const std::string& getLastError() const {
        return lastError_;
    }

    bool exists(const char* path) override {
        if (!mounted_) return false;
        return LittleFS.exists(path);
    }

    bool remove(const char* path) override {
        if (!mounted_) return false;
        return LittleFS.remove(path);
    }

    bool rename(const char* oldPath, const char* newPath) override {
        if (!mounted_) return false;
        return LittleFS.rename(oldPath, newPath);
    }

    size_t fileSize(const char* path) override {
        if (!mounted_ || !LittleFS.exists(path)) return 0;
        File f = LittleFS.open(path, "r");
        if (!f) return 0;
        size_t sz = f.size();
        f.close();
        return sz;
    }

    IFileHandle* open(const char* path, const char* mode) override {
        if (!mounted_ || !path || !mode) return nullptr;
        File f = LittleFS.open(path, mode);
        if (!f) return nullptr;
        return new LittleFSFileHandle(f, std::string(path), std::string(mode));
    }

    size_t totalBytes() override {
        if (!mounted_) return 0;
        return LittleFS.totalBytes();
    }

    size_t usedBytes() override {
        if (!mounted_) return 0;
        return LittleFS.usedBytes();
    }

private:
    bool mounted_ = false;
    std::string lastError_;
};

} // namespace VoiceJournal

#endif // ARDUINO
