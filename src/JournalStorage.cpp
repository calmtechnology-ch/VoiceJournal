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

    bool isKey(const char* key) const override {
        if (!opened_ || !key) return false;
        return prefs_.isKey(key);
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

namespace {

inline void fnv1aFeedByte(uint32_t& hash, uint8_t b) {
    hash ^= static_cast<uint32_t>(b);
    hash *= 16777619UL;
}

inline void fnv1aFeedUInt32(uint32_t& hash, uint32_t v) {
    fnv1aFeedByte(hash, static_cast<uint8_t>(v & 0xFFu));
    fnv1aFeedByte(hash, static_cast<uint8_t>((v >> 8) & 0xFFu));
    fnv1aFeedByte(hash, static_cast<uint8_t>((v >> 16) & 0xFFu));
    fnv1aFeedByte(hash, static_cast<uint8_t>((v >> 24) & 0xFFu));
}

} // namespace

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

void JournalStorage::formatLegacySlotKey(size_t slot, char* outBuf, size_t bufSize) {
    std::snprintf(outBuf, bufSize, "n%u", static_cast<unsigned>(slot));
}

void JournalStorage::formatBankSlotKey(uint8_t bank, size_t slot, char* outBuf, size_t bufSize) {
    const char prefix = (bank == 0) ? 'a' : 'b';
    std::snprintf(outBuf, bufSize, "%c%u", prefix, static_cast<unsigned>(slot));
}

const char* JournalStorage::bankHeadKey(uint8_t bank) {
    return (bank == 0) ? "ah" : "bh";
}

const char* JournalStorage::bankCountKey(uint8_t bank) {
    return (bank == 0) ? "ac" : "bc";
}

const char* JournalStorage::bankGenKey(uint8_t bank) {
    return (bank == 0) ? "ag" : "bg";
}

const char* JournalStorage::bankCommitKey(uint8_t bank) {
    return (bank == 0) ? "ak" : "bk";
}

bool JournalStorage::isSlotActive(size_t slot, size_t head, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (((head + i) % AppConfig::MAX_NOTES) == slot) {
            return true;
        }
    }
    return false;
}

uint32_t JournalStorage::computeChecksum(uint32_t gen,
                                         size_t head,
                                         size_t count,
                                         const std::array<String, AppConfig::MAX_NOTES>& notes) {
    uint32_t hash = 2166136261UL; // FNV-1a 32-bit offset basis
    fnv1aFeedUInt32(hash, kStorageMagicV2);
    fnv1aFeedUInt32(hash, gen);
    fnv1aFeedUInt32(hash, static_cast<uint32_t>(head));
    fnv1aFeedUInt32(hash, static_cast<uint32_t>(count));

    for (size_t i = 0; i < count; ++i) {
        const size_t slot = (head + i) % AppConfig::MAX_NOTES;
        const String& s = notes[slot];
        fnv1aFeedUInt32(hash, static_cast<uint32_t>(s.length()));
        const char* raw = s.c_str();
        for (size_t k = 0; k < s.length(); ++k) {
            fnv1aFeedByte(hash, static_cast<uint8_t>(raw[k]));
        }
    }

    // 0 est reserve pour indiquer un instantane non valide
    return (hash == 0) ? 1UL : hash;
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

bool JournalStorage::loadBank(uint8_t bank, Snapshot& outSnapshot) const {
    outSnapshot = Snapshot{};
    if (!store_) {
        return false;
    }

    const uint32_t commit = store_->getUInt(bankCommitKey(bank), 0);
    if (commit == 0) {
        return false;
    }

    const uint32_t gen = store_->getUInt(bankGenKey(bank), 0);
    const uint32_t storedHead = store_->getUInt(bankHeadKey(bank), static_cast<uint32_t>(AppConfig::MAX_NOTES));
    const uint32_t storedCount = store_->getUInt(bankCountKey(bank), static_cast<uint32_t>(AppConfig::MAX_NOTES + 1));

    if (gen == 0 || storedHead >= AppConfig::MAX_NOTES || storedCount > AppConfig::MAX_NOTES) {
        return false;
    }

    char keyBuf[8];
    for (size_t i = 0; i < storedCount; ++i) {
        const size_t slot = (storedHead + i) % AppConfig::MAX_NOTES;
        formatBankSlotKey(bank, slot, keyBuf, sizeof(keyBuf));
        if (!store_->isKey(keyBuf)) {
            return false;
        }
        String val = store_->getString(keyBuf, "");
        if (!isValidUtf8Length(val, AppConfig::MAX_NOTE_BYTES)) {
            return false;
        }
        outSnapshot.notes[slot] = val;
    }

    const uint32_t expected = computeChecksum(gen, storedHead, storedCount, outSnapshot.notes);
    if (commit != expected) {
        return false;
    }

    outSnapshot.head = storedHead;
    outSnapshot.count = storedCount;
    outSnapshot.generation = gen;
    outSnapshot.valid = true;
    return true;
}

bool JournalStorage::commitSnapshot(uint8_t bank,
                                    uint32_t gen,
                                    const std::array<String, AppConfig::MAX_NOTES>& notes,
                                    size_t head,
                                    size_t count) {
    if (!store_) {
        lastError_ = "Backend NVS absent";
        return false;
    }

    // 1. Invalider le marqueur de validation de la banque cible AVANT toute modification.
    //    En cas de coupure d'alimentation aux etapes suivantes, la banque active precedente reste intacte.
    if (!store_->putUInt(bankCommitKey(bank), 0)) {
        lastError_ = "Echec invalidation marqueur banque NVS";
        return false;
    }

    // 2. Ecrire les notes actives et supprimer les emplacements inutilises dans la banque cible.
    char keyBuf[8];
    for (size_t slot = 0; slot < AppConfig::MAX_NOTES; ++slot) {
        formatBankSlotKey(bank, slot, keyBuf, sizeof(keyBuf));
        if (isSlotActive(slot, head, count)) {
            if (!store_->putString(keyBuf, notes[slot])) {
                lastError_ = "Echec ecriture note NVS";
                return false;
            }
        } else if (store_->isKey(keyBuf)) {
            if (!store_->remove(keyBuf)) {
                lastError_ = "Echec suppression emplacement NVS";
                return false;
            }
        }
    }

    // 3. Ecrire les metadonnees (index circulaire, compteur, generation, reference active).
    if (!store_->putUInt(bankHeadKey(bank), static_cast<uint32_t>(head))) {
        lastError_ = "Echec mise a jour index circulaire NVS";
        return false;
    }
    if (!store_->putUInt(bankCountKey(bank), static_cast<uint32_t>(count))) {
        lastError_ = "Echec mise a jour compteur NVS";
        return false;
    }
    if (!store_->putUInt(bankGenKey(bank), gen)) {
        lastError_ = "Echec mise a jour generation NVS";
        return false;
    }
    if (!store_->putUInt(kKeyActive, static_cast<uint32_t>(bank))) {
        lastError_ = "Echec mise a jour reference active NVS";
        return false;
    }

    // 4. Ecrire le marqueur de validation FNV-1a EN DERNIER.
    const uint32_t checksum = computeChecksum(gen, head, count, notes);
    if (!store_->putUInt(bankCommitKey(bank), checksum)) {
        lastError_ = "Echec validation instantane NVS";
        return false;
    }

    return true;
}

size_t JournalStorage::salvageBankNotes(uint8_t bank,
                                        std::array<String, AppConfig::MAX_NOTES>& outNotes) const {
    size_t recovered = 0;
    char keyBuf[8];
    for (size_t slot = 0; slot < AppConfig::MAX_NOTES && recovered < AppConfig::MAX_NOTES; ++slot) {
        formatBankSlotKey(bank, slot, keyBuf, sizeof(keyBuf));
        if (!store_->isKey(keyBuf)) {
            continue;
        }
        String val = store_->getString(keyBuf, "");
        val.trim();
        if (val.length() > AppConfig::MAX_NOTE_BYTES) {
            val = truncateUtf8(val, AppConfig::MAX_NOTE_BYTES);
        }
        if (isValidUtf8Length(val, AppConfig::MAX_NOTE_BYTES)) {
            outNotes[recovered++] = val;
        }
    }
    return recovered;
}

bool JournalStorage::migrateFromV1() {
    const uint32_t storedHead = store_->getUInt(kKeyHeadV1, 0);
    const uint32_t storedCount = store_->getUInt(kKeyCountV1, 0);

    std::array<String, AppConfig::MAX_NOTES> migrated;
    size_t validCount = 0;
    char keyBuf[8];
    bool metadataCorrupted = (storedHead >= AppConfig::MAX_NOTES || storedCount > AppConfig::MAX_NOTES);

    if (!metadataCorrupted) {
        for (size_t i = 0; i < storedCount; ++i) {
            const size_t slot = (storedHead + i) % AppConfig::MAX_NOTES;
            formatLegacySlotKey(slot, keyBuf, sizeof(keyBuf));
            if (!store_->isKey(keyBuf)) {
                metadataCorrupted = true;
                continue;
            }
            String val = store_->getString(keyBuf, "");
            val.trim();
            if (val.length() > AppConfig::MAX_NOTE_BYTES) {
                val = truncateUtf8(val, AppConfig::MAX_NOTE_BYTES);
            }
            if (isValidUtf8Length(val, AppConfig::MAX_NOTE_BYTES) && validCount < AppConfig::MAX_NOTES) {
                migrated[validCount++] = val;
            } else {
                metadataCorrupted = true;
            }
        }
    } else {
        // Metadonnees v0.1.0 hors bornes : recuperer toutes les notes encore valides sans effacer silencieusement
        for (size_t slot = 0; slot < AppConfig::MAX_NOTES && validCount < AppConfig::MAX_NOTES; ++slot) {
            formatLegacySlotKey(slot, keyBuf, sizeof(keyBuf));
            if (!store_->isKey(keyBuf)) {
                continue;
            }
            String val = store_->getString(keyBuf, "");
            val.trim();
            if (val.length() > AppConfig::MAX_NOTE_BYTES) {
                val = truncateUtf8(val, AppConfig::MAX_NOTE_BYTES);
            }
            if (isValidUtf8Length(val, AppConfig::MAX_NOTE_BYTES)) {
                migrated[validCount++] = val;
            }
        }
    }

    // Persister l'instantane migre dans la banque 0 (generation 1) avant d'ecrire le magic v2.
    // En cas de coupure avant l'ecriture de kKeyMagicV2, les cles v0.1.0 restent intactes.
    if (!commitSnapshot(0, 1, migrated, 0, validCount)) {
        lastError_ = String("Echec migration NVS v0.1.0: ") + lastError_;
        return false;
    }

    if (!store_->putUInt(kKeyMagicV2, kStorageMagicV2)) {
        lastError_ = "Echec finalisation migration NVS v0.1.0.1";
        return false;
    }

    notes_ = migrated;
    head_ = 0;
    count_ = validCount;
    activeBank_ = 0;
    generation_ = 1;

    if (metadataCorrupted) {
        recoveryDiagnostic_ = "Migration v0.1.0 avec recuperation partielle des metadonnees";
        Serial.printf("[VJ][STORAGE] %s (%u notes recuperees)\n",
                      recoveryDiagnostic_.c_str(),
                      static_cast<unsigned>(validCount));
    } else {
        recoveryDiagnostic_ = "Migration NVS v0.1.0 -> v0.1.0.1 reussie";
        Serial.printf("[VJ][STORAGE] %s (%u notes preservees)\n",
                      recoveryDiagnostic_.c_str(),
                      static_cast<unsigned>(validCount));
    }

    return true;
}

bool JournalStorage::begin() {
    lastError_ = "";
    recoveryDiagnostic_ = "";
    initialized_ = false;
    head_ = 0;
    count_ = 0;
    activeBank_ = 0;
    generation_ = 0;
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

    const uint32_t magicV2 = store_->getUInt(kKeyMagicV2, 0);
    if (magicV2 == kStorageMagicV2) {
        Snapshot snapA;
        Snapshot snapB;
        const bool okA = loadBank(0, snapA);
        const bool okB = loadBank(1, snapB);

        if (okA || okB) {
            uint8_t chosenBank = 0;
            if (okA && okB) {
                if (snapB.generation > snapA.generation) {
                    chosenBank = 1;
                } else if (snapA.generation > snapB.generation) {
                    chosenBank = 0;
                } else {
                    const uint32_t actHint = store_->getUInt(kKeyActive, 0);
                    chosenBank = (actHint == 1) ? 1 : 0;
                }
            } else if (okA) {
                chosenBank = 0;
                if (store_->isKey(bankGenKey(1)) || store_->isKey(bankCommitKey(1))) {
                    recoveryDiagnostic_ = "Banque B incomplete/invalide ignoree, reprise sur banque A";
                    Serial.printf("[VJ][STORAGE] %s\n", recoveryDiagnostic_.c_str());
                }
            } else {
                chosenBank = 1;
                if (store_->isKey(bankGenKey(0)) || store_->isKey(bankCommitKey(0))) {
                    recoveryDiagnostic_ = "Banque A incomplete/invalide ignoree, reprise sur banque B";
                    Serial.printf("[VJ][STORAGE] %s\n", recoveryDiagnostic_.c_str());
                }
            }

            const Snapshot& chosen = (chosenBank == 0) ? snapA : snapB;
            notes_ = chosen.notes;
            head_ = chosen.head;
            count_ = chosen.count;
            activeBank_ = chosenBank;
            generation_ = chosen.generation;
            initialized_ = true;
            return true;
        }

        // Aucune des deux banques n'est valide : corruption reelle.
        // Ne pas reinitialiser silencieusement : tenter de sauver les notes valides.
        const uint32_t actHint = store_->getUInt(kKeyActive, 0);
        const uint8_t primaryBank = (actHint == 1) ? 1 : 0;
        const uint8_t secondaryBank = static_cast<uint8_t>(1 - primaryBank);

        std::array<String, AppConfig::MAX_NOTES> salvaged;
        size_t salvagedCount = salvageBankNotes(primaryBank, salvaged);
        if (salvagedCount == 0) {
            salvagedCount = salvageBankNotes(secondaryBank, salvaged);
        }

        if (salvagedCount > 0) {
            const uint32_t nextGen = 1;
            if (!commitSnapshot(0, nextGen, salvaged, 0, salvagedCount)) {
                lastError_ = String("Corruption NVS et echec recuperation: ") + lastError_;
                Serial.printf("[VJ][STORAGE][ERROR] %s\n", lastError_.c_str());
                store_->close();
                return false;
            }
            notes_ = salvaged;
            head_ = 0;
            count_ = salvagedCount;
            activeBank_ = 0;
            generation_ = nextGen;
            recoveryDiagnostic_ = "Corruption metadonnees NVS : observations valides recuperees";
            Serial.printf("[VJ][STORAGE] %s (%u notes)\n",
                          recoveryDiagnostic_.c_str(),
                          static_cast<unsigned>(salvagedCount));
            initialized_ = true;
            return true;
        }

        lastError_ = "Corruption NVS : aucun instantane valide recuperable";
        Serial.printf("[VJ][STORAGE][ERROR] %s\n", lastError_.c_str());
        store_->close();
        return false;
    }

    // Verifier presence d'un carnet au format v0.1.0 pour migration non destructive
    const uint32_t magicV1 = store_->getUInt(kKeyMagicV1, 0);
    if (magicV1 == kStorageMagicV1) {
        if (!migrateFromV1()) {
            Serial.printf("[VJ][STORAGE][ERROR] %s\n", lastError_.c_str());
            store_->close();
            return false;
        }
        initialized_ = true;
        return true;
    }

    // Espace NVS corrompu avec magic inconnu : ne pas effacer silencieusement
    if (magicV1 != 0 || magicV2 != 0) {
        lastError_ = "Signature NVS inconnue ou corrompue";
        Serial.printf("[VJ][STORAGE][ERROR] %s (v1=0x%08X, v2=0x%08X)\n",
                      lastError_.c_str(),
                      static_cast<unsigned>(magicV1),
                      static_cast<unsigned>(magicV2));
        store_->close();
        return false;
    }

    // Premier demarrage (espace vierge) : creer un instantane vide valide dans banque 0
    std::array<String, AppConfig::MAX_NOTES> emptyNotes;
    if (!commitSnapshot(0, 1, emptyNotes, 0, 0) ||
        !store_->putUInt(kKeyMagicV2, kStorageMagicV2)) {
        if (lastError_.length() == 0) {
            lastError_ = "Echec initialisation metadonnees NVS";
        }
        store_->close();
        return false;
    }

    activeBank_ = 0;
    generation_ = 1;
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

    std::array<String, AppConfig::MAX_NOTES> candidateNotes = notes_;
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

    candidateNotes[targetSlot] = cleaned;

    const uint8_t targetBank = static_cast<uint8_t>(1 - activeBank_);
    const uint32_t nextGen = generation_ + 1;

    if (!commitSnapshot(targetBank, nextGen, candidateNotes, newHead, newCount)) {
        // Echec NVS : la banque active et la RAM restent integralement preservees
        return false;
    }

    // Confirmer en RAM uniquement apres validation complete de l'instantane NVS
    notes_ = candidateNotes;
    head_ = newHead;
    count_ = newCount;
    activeBank_ = targetBank;
    generation_ = nextGen;
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

    std::array<String, AppConfig::MAX_NOTES> emptyNotes;
    const uint8_t oldBank = activeBank_;
    const uint8_t targetBank = static_cast<uint8_t>(1 - activeBank_);
    const uint32_t nextGen = generation_ + 1;

    // 1. Valider d'abord l'instantane vide dans la banque inactive.
    //    Si une suppression de cle ou une ecriture echoue ici, l'ancien etat reste intact en RAM et en NVS.
    if (!commitSnapshot(targetBank, nextGen, emptyNotes, 0, 0)) {
        return false;
    }

    // 2. Des lors que l'instantane vide de generation superieure est valide, la RAM reflete
    //    immediatement cet etat persistant (aucune contradiction RAM/NVS, aucune resurrection au reboot).
    notes_ = emptyNotes;
    head_ = 0;
    count_ = 0;
    activeBank_ = targetBank;
    generation_ = nextGen;

    // 3. Nettoyer l'ancienne banque et verifier chaque suppression NVS.
    bool cleanupOk = true;
    if (store_->isKey(bankCommitKey(oldBank))) {
        if (!store_->putUInt(bankCommitKey(oldBank), 0)) {
            cleanupOk = false;
        }
    }
    char keyBuf[8];
    for (size_t slot = 0; slot < AppConfig::MAX_NOTES; ++slot) {
        formatBankSlotKey(oldBank, slot, keyBuf, sizeof(keyBuf));
        if (store_->isKey(keyBuf)) {
            if (!store_->remove(keyBuf)) {
                cleanupOk = false;
            }
        }
    }

    if (!cleanupOk) {
        lastError_ = "Echec suppression complete des anciennes cles NVS";
        return false;
    }

    return true;
}
