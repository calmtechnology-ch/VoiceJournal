#pragma once

#include <cstdint>
#include <map>
#include <string>
#include "JournalStorage.h"

/**
 * @brief Backend NVS en memoire pour les tests natifs.
 *        Permet de simuler un redemarrage (en conservant les maps) et des echecs d'ecriture.
 */
class MockKeyValueStore final : public IKeyValueStore {
public:
    bool open(const char* ns, bool readOnly) override {
        if (failOpen) return false;
        namespaceName = ns ? ns : "";
        opened = true;
        (void)readOnly;
        return true;
    }

    void close() override {
        opened = false;
    }

    uint32_t getUInt(const char* key, uint32_t defaultValue = 0) const override {
        if (!opened || !key) return defaultValue;
        auto it = uints.find(key);
        return (it != uints.end()) ? it->second : defaultValue;
    }

    bool putUInt(const char* key, uint32_t value) override {
        if (!opened || !key || failNextWrite || failAllWrites) {
            failNextWrite = false;
            return false;
        }
        uints[key] = value;
        return true;
    }

    String getString(const char* key, const String& defaultValue = "") const override {
        if (!opened || !key) return defaultValue;
        auto it = strings.find(key);
        return (it != strings.end()) ? String(it->second.c_str()) : defaultValue;
    }

    bool putString(const char* key, const String& value) override {
        if (!opened || !key || failNextWrite || failAllWrites) {
            failNextWrite = false;
            return false;
        }
        strings[key] = value.c_str();
        return true;
    }

    bool remove(const char* key) override {
        if (!opened || !key) return false;
        strings.erase(key);
        uints.erase(key);
        return true;
    }

    bool opened = false;
    std::string namespaceName;
    std::map<std::string, uint32_t> uints;
    std::map<std::string, std::string> strings;

    bool failOpen = false;
    bool failNextWrite = false;
    bool failAllWrites = false;
};
