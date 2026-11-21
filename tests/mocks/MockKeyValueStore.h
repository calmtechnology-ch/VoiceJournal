#pragma once

#include <cstdint>
#include <map>
#include <string>
#include "JournalStorage.h"

/**
 * @brief Backend NVS en memoire pour les tests natifs.
 *        Permet de simuler :
 *        - un redemarrage (en conservant les maps),
 *        - un echec d'ecriture ou de suppression cible sur une cle precise,
 *        - une coupure d'alimentation apres exactement N operations persistantes.
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

    bool isKey(const char* key) const override {
        if (!opened || !key) return false;
        return (uints.find(key) != uints.end()) || (strings.find(key) != strings.end());
    }

    uint32_t getUInt(const char* key, uint32_t defaultValue = 0) const override {
        if (!opened || !key) return defaultValue;
        auto it = uints.find(key);
        return (it != uints.end()) ? it->second : defaultValue;
    }

    bool putUInt(const char* key, uint32_t value) override {
        if (!opened || !key) return false;
        if (shouldFailWrite(key)) {
            return false;
        }
        uints[key] = value;
        ++writeOpsCount;
        return true;
    }

    String getString(const char* key, const String& defaultValue = "") const override {
        if (!opened || !key) return defaultValue;
        auto it = strings.find(key);
        return (it != strings.end()) ? String(it->second.c_str()) : defaultValue;
    }

    bool putString(const char* key, const String& value) override {
        if (!opened || !key) return false;
        if (shouldFailWrite(key)) {
            return false;
        }
        strings[key] = value.c_str();
        ++writeOpsCount;
        return true;
    }

    bool remove(const char* key) override {
        if (!opened || !key) return false;
        if (shouldFailRemove(key)) {
            return false;
        }
        const bool existed = (strings.erase(key) > 0) || (uints.erase(key) > 0);
        ++writeOpsCount;
        return existed;
    }

    void resetFaultInjection() {
        failOpen = false;
        failNextWrite = false;
        failAllWrites = false;
        failNextRemove = false;
        failAllRemoves = false;
        failOnKey.clear();
        failRemoveOnKey.clear();
        powerCutAfterOps = -1;
        powerCutTriggered = false;
    }

    bool opened = false;
    std::string namespaceName;
    std::map<std::string, uint32_t> uints;
    std::map<std::string, std::string> strings;

    // Compteurs d'observations
    size_t writeOpsCount = 0;

    // Injections d'echecs
    bool failOpen = false;
    bool failNextWrite = false;
    bool failAllWrites = false;
    bool failNextRemove = false;
    bool failAllRemoves = false;
    std::string failOnKey;
    std::string failRemoveOnKey;

    // Coupure d'alimentation simulee : echoue a partir de la N-ieme operation persistante
    int powerCutAfterOps = -1;
    bool powerCutTriggered = false;

private:
    bool checkPowerCut() {
        if (powerCutAfterOps >= 0) {
            if (powerCutAfterOps == 0) {
                powerCutTriggered = true;
                return true;
            }
            --powerCutAfterOps;
        }
        return false;
    }

    bool shouldFailWrite(const char* key) {
        if (powerCutTriggered || checkPowerCut()) {
            return true;
        }
        if (failAllWrites) {
            return true;
        }
        if (failNextWrite) {
            failNextWrite = false;
            return true;
        }
        if (!failOnKey.empty() && failOnKey == key) {
            return true;
        }
        return false;
    }

    bool shouldFailRemove(const char* key) {
        if (powerCutTriggered || checkPowerCut()) {
            return true;
        }
        if (failAllRemoves) {
            return true;
        }
        if (failNextRemove) {
            failNextRemove = false;
            return true;
        }
        if (!failRemoveOnKey.empty() && failRemoveOnKey == key) {
            return true;
        }
        return false;
    }
};
