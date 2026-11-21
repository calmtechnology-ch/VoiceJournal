#include "JournalStorage.h"

#include <cstdio>
#include <cstring>

#ifdef ARDUINO
#include <Preferences.h>

namespace {

class NvsKeyValueStore final : public IKeyValueStore {
public:
    bool open(const char* ns, bool readOnly) override {
        opened_ = prefs_.begin(ns, readOnly);
        return opened_;
    }

    void close() override {
        if (opened_) {
            prefs_.end();
            opened_ = false;
        }
    }

    uint32_t getUInt(const char* key, uint32_t defaultValue) const override {
        if (!opened_) return defaultValue;
        return prefs_.getUInt(key, defaultValue);
    }

    bool putUInt(const char* key, uint32_t value) override {
        if (!opened_) return false;
        return prefs_.putUInt(key, value) > 0;
    }

    String getString(const char* key, const String& defaultValue) const override {
        if (!opened_) return defaultValue;
        return prefs_.getString(key, defaultValue);
    }

    bool putString(const char* key, const String& value) override {
        if (!opened_) return false;
        // Preferences::putString retourne le nombre d'octets ecrits (0 en cas d'echec)
        return prefs_.putString(key, value) == value.length();
    }

    bool remove(const char* key) override {
        if (!opened_) return false;
        return prefs_.remove(key);
    }

private:
    mutable Preferences prefs_;
    bool opened_ = false;
};

} // namespace
#endif

JournalStorage::JournalStorage() {
#ifdef ARDUINO
    store_ = new NvsKeyValueStore();
    ownsStore_ = true;
#else
    store_ = nullptr;
    ownsStore_ = false;
#endif
}

JournalStorage::JournalStorage(IKeyValueStore* externalStore)
    : store_(externalStore), ownsStore_(false) {}

JournalStorage::~JournalStorage() {
    if (store_) {
        if (initialized_) {
            store_->close();
        }
        if (ownsStore_) {
            delete store_;
        }
        store_ = nullptr;
    }
}

void JournalStorage::formatSlotKey(size_t slot, char* outBuf, size_t bufSize) {
    std::snprintf(outBuf, bufSize, "n%u", static_cast<unsigned>(slot));
}

String JournalStorage::truncateUtf8(const String& text, size_t maxBytes) {
    const size_t len = text.length();
    if (len <= maxBytes) {
        return text;
    }
    if (maxBytes == 0) {
        return "";
    }

    const char* raw = text.c_str();
    size_t cut = maxBytes;

    // Reculer tant que l'octet a la position `cut` est un octet de continuation UTF-8 (10xxxxxx)
    while (cut > 0 && (static_cast<uint8_t>(raw[cut]) & 0xC0u) == 0x80u) {
        --cut;
    }

    // Verifier si le caractere debutant juste avant la coupe est complet
    if (cut > 0) {
        size_t pos = cut - 1;
        while (pos > 0 && (static_cast<uint8_t>(raw[pos]) & 0xC0u) == 0x80u) {
            --pos;
        }
        const uint8_t lead = static_cast<uint8_t>(raw[pos]);
        size_t charLen = 1;
        if ((lead & 0x80u) == 0x00u)      charLen = 1;
        else if ((lead & 0xE0u) == 0xC0u) charLen = 2;
        else if ((lead & 0xF0u) == 0xE0u) charLen = 3;
        else if ((lead & 0xF8u) == 0xF0u) charLen = 4;

        if (pos + charLen > maxBytes) {
            cut = pos;
        }
    }

    return text.substring(0, cut);
}

bool JournalStorage::isValidUtf8Length(const String& text, size_t maxBytes) {
    const size_t len = text.length();
    if (len == 0 || len > maxBytes) {
        return false;
    }
    const char* raw = text.c_str();
    size_t i = 0;
    while (i < len) {
        const uint8_t c = static_cast<uint8_t>(raw[i]);
        size_t seqLen = 1;
        if ((c & 0x80u) == 0x00u)      seqLen = 1;
        else if ((c & 0xE0u) == 0xC0u) seqLen = 2;
        else if ((c & 0xF0u) == 0xE0u) seqLen = 3;
        else if ((c & 0xF8u) == 0xF0u) seqLen = 4;
        else return false; // Octet de continuation orphelin ou invalide

        if (i + seqLen > len) {
            return false; // Sequence UTF-8 tronquee
        }
        for (size_t k = 1; k < seqLen; ++k) {
            if ((static_cast<uint8_t>(raw[i + k]) & 0xC0u) != 0x80u) {
                return false;
            }
        }
        i += seqLen;
    }
    return true;
}

bool JournalStorage::begin() {
    lastError_ = "";
    initialized_ = false;
    head_ = 0;
    count_ = 0;
    for (size_t i = 0; i < AppConfig::MAX_NOTES; ++i) {
        notes_[i] = "";
    }

    if (!store_) {
        lastError_ = "Backend NVS absent";
        return false;
    }

    if (!store_->open(AppConfig::NVS_NAMESPACE, false)) {
        lastError_ = "Echec ouverture NVS";
        return false;
    }

    const uint32_t magic = store_->getUInt(kKeyMagic, 0);
    if (magic != kStorageMagic) {
        // Premier demarrage ou espace vierge : initialiser proprement les metadonnees
        if (!store_->putUInt(kKeyHead, 0) ||
            !store_->putUInt(kKeyCount, 0) ||
            !store_->putUInt(kKeyMagic, kStorageMagic)) {
            lastError_ = "Echec initialisation metadonnees NVS";
            store_->close();
            return false;
        }
        initialized_ = true;
        return true;
    }

    const uint32_t storedHead  = store_->getUInt(kKeyHead, 0);
    const uint32_t storedCount = store_->getUInt(kKeyCount, 0);

    // Verification stricte des bornes pour recuperation propre apres coupure d'alimentation
    if (storedHead >= AppConfig::MAX_NOTES || storedCount > AppConfig::MAX_NOTES) {
        lastError_ = "Metadonnees NVS corrompues, reinitialisation";
        head_ = 0;
        count_ = 0;
        store_->putUInt(kKeyHead, 0);
        store_->putUInt(kKeyCount, 0);
        initialized_ = true;
        return true;
    }

    // Charger uniquement les notes valides en preservant l'ordre chronologique
    std::array<String, AppConfig::MAX_NOTES> recovered;
    size_t validCount = 0;
    char keyBuf[8];

    for (size_t i = 0; i < storedCount; ++i) {
        const size_t slot = (storedHead + i) % AppConfig::MAX_NOTES;
        formatSlotKey(slot, keyBuf, sizeof(keyBuf));
        String val = store_->getString(keyBuf, "");
        val.trim();
        if (val.length() > 0) {
            if (val.length() > AppConfig::MAX_NOTE_BYTES) {
                val = truncateUtf8(val, AppConfig::MAX_NOTE_BYTES);
            }
            if (val.length() > 0 && validCount < AppConfig::MAX_NOTES) {
                recovered[validCount++] = val;
            }
        }
    }

    if (validCount == storedCount) {
        head_ = storedHead;
        count_ = storedCount;
        for (size_t i = 0; i < count_; ++i) {
            const size_t slot = (head_ + i) % AppConfig::MAX_NOTES;
            notes_[slot] = recovered[i];
        }
    } else {
        // Recompacter proprement si un emplacement etait manquant/vide apres coupure
        head_ = 0;
        count_ = validCount;
        for (size_t i = 0; i < validCount; ++i) {
            notes_[i] = recovered[i];
            formatSlotKey(i, keyBuf, sizeof(keyBuf));
            store_->putString(keyBuf, notes_[i]);
        }
        store_->putUInt(kKeyHead, 0);
        store_->putUInt(kKeyCount, static_cast<uint32_t>(validCount));
    }

    initialized_ = true;
    return true;
}

bool JournalStorage::addNote(const String& text) {
    lastError_ = "";
    if (!initialized_ || !store_) {
        lastError_ = "Stockage non initialise";
        return false;
    }

    String cleaned = text;
    cleaned.trim();

    if (cleaned.length() == 0) {
        lastError_ = "Note vide refusee";
        return false;
    }

    if (cleaned.length() > AppConfig::MAX_NOTE_BYTES) {
        lastError_ = "Note trop longue (> 180 octets UTF-8)";
        return false;
    }

    if (!isValidUtf8Length(cleaned, AppConfig::MAX_NOTE_BYTES)) {
        lastError_ = "Sequence UTF-8 invalide";
        return false;
    }

    size_t targetSlot = 0;
    size_t newHead = head_;
    size_t newCount = count_;

    if (count_ < AppConfig::MAX_NOTES) {
        targetSlot = (head_ + count_) % AppConfig::MAX_NOTES;
        newCount = count_ + 1;
    } else {
        // Tampon plein : ecraser la plus ancienne note (a head_)
        targetSlot = head_;
        newHead = (head_ + 1) % AppConfig::MAX_NOTES;
        newCount = AppConfig::MAX_NOTES;
    }

    // 1. Ecrire d'abord la charge utile en NVS
    char keyBuf[8];
    formatSlotKey(targetSlot, keyBuf, sizeof(keyBuf));
    if (!store_->putString(keyBuf, cleaned)) {
        lastError_ = "Echec ecriture note NVS";
        return false;
    }

    // 2. Mettre a jour les metadonnees en NVS (head puis count)
    if (newHead != head_) {
        if (!store_->putUInt(kKeyHead, static_cast<uint32_t>(newHead))) {
            // Restaurer l'ancienne note dans le slot si possible
            if (notes_[targetSlot].length() > 0) {
                store_->putString(keyBuf, notes_[targetSlot]);
            }
            lastError_ = "Echec mise a jour head NVS";
            return false;
        }
    }

    if (newCount != count_) {
        if (!store_->putUInt(kKeyCount, static_cast<uint32_t>(newCount))) {
            // Annuler le changement de head si necessaire
            if (newHead != head_) {
                store_->putUInt(kKeyHead, static_cast<uint32_t>(head_));
            }
            lastError_ = "Echec mise a jour count NVS";
            return false;
        }
    }

    // 3. Confirmer en RAM uniquement apres succes complet NVS
    notes_[targetSlot] = cleaned;
    head_ = newHead;
    count_ = newCount;
    return true;
}

String JournalStorage::getNote(size_t index) const {
    if (!initialized_ || index >= count_) {
        return "";
    }
    const size_t slot = (head_ + index) % AppConfig::MAX_NOTES;
    return notes_[slot];
}

String JournalStorage::getLastNote() const {
    if (!initialized_ || count_ == 0) {
        return "";
    }
    return getNote(count_ - 1);
}

String JournalStorage::getAllNotes() const {
    if (!initialized_ || count_ == 0) {
        return "";
    }
    String result;
    result.reserve(count_ * 64);
    for (size_t i = 0; i < count_; ++i) {
        if (i > 0) {
            result += "\n";
        }
        result += String(static_cast<unsigned>(i + 1));
        result += ". ";
        result += getNote(i);
    }
    return result;
}

bool JournalStorage::clear() {
    lastError_ = "";
    if (!initialized_ || !store_) {
        lastError_ = "Stockage non initialise";
        return false;
    }

    // Mettre count=0 et head=0 en NVS en premier pour garantir un etat vide coherent
    if (!store_->putUInt(kKeyCount, 0)) {
        lastError_ = "Echec effacement count NVS";
        return false;
    }
    if (!store_->putUInt(kKeyHead, 0)) {
        lastError_ = "Echec effacement head NVS";
        return false;
    }

    char keyBuf[8];
    for (size_t i = 0; i < AppConfig::MAX_NOTES; ++i) {
        formatSlotKey(i, keyBuf, sizeof(keyBuf));
        store_->remove(keyBuf);
        notes_[i] = "";
    }

    head_ = 0;
    count_ = 0;
    return true;
}
