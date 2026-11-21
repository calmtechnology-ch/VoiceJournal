#pragma once

#include <Arduino.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include "AppConfig.h"

/**
 * @brief Interface minimale d'acces cle/valeur pour decoupler NVS (Preferences)
 *        des tests natifs sans alourdir l'architecture de production.
 */
class IKeyValueStore {
public:
    virtual ~IKeyValueStore() = default;
    virtual bool open(const char* ns, bool readOnly) = 0;
    virtual void close() = 0;
    virtual bool isKey(const char* key) const = 0;
    virtual uint32_t getUInt(const char* key, uint32_t defaultValue = 0) const = 0;
    virtual bool putUInt(const char* key, uint32_t value) = 0;
    virtual String getString(const char* key, const String& defaultValue = "") const = 0;
    virtual bool putString(const char* key, const String& value) = 0;
    virtual bool remove(const char* key) = 0;
};

/**
 * @class JournalStorage
 * @brief Stockage persistant local des observations dans la NVS ESP32 (namespace "vjournal").
 *
 * Strategie v0.1.0.1 (double instantane A/B sans pretendre a une atomicite materielle multi-cles) :
 * - Deux banques independantes ("a" et "b") contenant chacune les 5 emplacements
 *   ("a0".."a4" / "b0".."b4"), l'index circulaire ("ah"/"bh"), le compteur ("ac"/"bc"),
 *   un numero de generation monotone ("ag"/"bg") et un marqueur de validation FNV-1a ("ak"/"bk")
 *   ecrit EN DERNIER.
 * - Chaque ecriture (addNote / clear) cible exclusivement la banque inactive apres avoir
 *   invalide son marqueur de validation, laissant la banque active intacte en cas de coupure.
 * - Au demarrage (begin), les deux banques sont verifiees ; la banque valide de generation
 *   la plus elevee est selectionnee.
 * - Migration automatique et non destructive depuis le format v0.1.0 ("magic" = 0x564A3031,
 *   "head", "count", "n0".."n4").
 * - Longueur bornee a AppConfig::MAX_NOTE_BYTES (180 octets UTF-8).
 */
class JournalStorage {
public:
    /**
     * @brief Constructeur de production (utilise Preferences / NVS ESP32).
     */
    JournalStorage();

    /**
     * @brief Constructeur avec backend injecte (tests unitaires).
     *        L'appelant conserve la propriete du pointeur store.
     */
    explicit JournalStorage(IKeyValueStore* externalStore);

    ~JournalStorage();

    JournalStorage(const JournalStorage&) = delete;
    JournalStorage& operator=(const JournalStorage&) = delete;

    /**
     * @brief Initialise l'espace NVS "vjournal" et charge les observations valides en RAM.
     */
    bool begin();

    /**
     * @brief Ajoute une observation dans le tampon circulaire et la persiste en NVS.
     *        Refuse les chaines vides, les chaines > MAX_NOTE_BYTES octets UTF-8
     *        et tout echec d'ecriture NVS.
     * @return true uniquement apres confirmation effective de l'ecriture NVS.
     */
    bool addNote(const String& text);

    /**
     * @brief Retourne l'observation a l'index chronologique demande (0 = plus ancienne).
     */
    String getNote(size_t index) const;

    /**
     * @brief Retourne l'observation la plus recente (ou chaine vide si carnet vide).
     */
    String getLastNote() const;

    /**
     * @brief Nombre d'observations actuellement enregistrees (0..MAX_NOTES).
     */
    size_t count() const { return count_; }

    /**
     * @brief Capacite maximale du carnet.
     */
    static constexpr size_t capacity() { return AppConfig::MAX_NOTES; }

    /**
     * @brief Retourne toutes les observations numerotees dans l'ordre chronologique.
     */
    String getAllNotes() const;

    /**
     * @brief Efface toutes les observations en NVS et en RAM.
     * @return true uniquement si la persistance NVS a reussi.
     */
    bool clear();

    /**
     * @brief Indique si le stockage a ete initialise avec succes.
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Retourne la derniere erreur de stockage (chaine vide si aucune).
     */
    const String& getLastError() const { return lastError_; }

    /**
     * @brief Retourne le diagnostic de recuperation ou migration issu du dernier begin().
     */
    const String& getRecoveryDiagnostic() const { return recoveryDiagnostic_; }

    /**
     * @brief Indique la banque active courante (0 = A, 1 = B) pour diagnostic et tests.
     */
    uint8_t getActiveBank() const { return activeBank_; }

    /**
     * @brief Indique la generation courante du dernier instantane valide.
     */
    uint32_t getGeneration() const { return generation_; }

    /**
     * @brief Tronque une chaine UTF-8 a au plus maxBytes octets sans jamais
     *        couper au milieu d'une sequence multioctet UTF-8.
     */
    static String truncateUtf8(const String& text, size_t maxBytes);

    /**
     * @brief Verifie si une chaine UTF-8 a une longueur valide (1..maxBytes)
     *        et ne se termine pas sur une sequence UTF-8 tronquee.
     */
    static bool isValidUtf8Length(const String& text, size_t maxBytes);

private:
    // Format v0.1.0 (legacy, conserve en lecture pour migration transparente)
    static constexpr uint32_t kStorageMagicV1 = 0x564A3031UL; // "VJ01"
    static constexpr const char* kKeyMagicV1  = "magic";
    static constexpr const char* kKeyHeadV1   = "head";
    static constexpr const char* kKeyCountV1  = "count";

    // Format v0.1.0.1 (double instantane A/B avec marqueur de validation FNV-1a)
    static constexpr uint32_t kStorageMagicV2 = 0x564A3032UL; // "VJ02"
    static constexpr const char* kKeyMagicV2  = "v2";
    static constexpr const char* kKeyActive   = "act";

    struct Snapshot {
        std::array<String, AppConfig::MAX_NOTES> notes;
        size_t head = 0;
        size_t count = 0;
        uint32_t generation = 0;
        bool valid = false;
    };

    static void formatLegacySlotKey(size_t slot, char* outBuf, size_t bufSize);
    static void formatBankSlotKey(uint8_t bank, size_t slot, char* outBuf, size_t bufSize);
    static const char* bankHeadKey(uint8_t bank);
    static const char* bankCountKey(uint8_t bank);
    static const char* bankGenKey(uint8_t bank);
    static const char* bankCommitKey(uint8_t bank);

    static bool isSlotActive(size_t slot, size_t head, size_t count);
    static uint32_t computeChecksum(uint32_t gen,
                                    size_t head,
                                    size_t count,
                                    const std::array<String, AppConfig::MAX_NOTES>& notes);

    bool loadBank(uint8_t bank, Snapshot& outSnapshot) const;
    bool commitSnapshot(uint8_t bank,
                        uint32_t gen,
                        const std::array<String, AppConfig::MAX_NOTES>& notes,
                        size_t head,
                        size_t count);
    bool migrateFromV1();
    size_t salvageBankNotes(uint8_t bank, std::array<String, AppConfig::MAX_NOTES>& outNotes) const;

    IKeyValueStore* store_ = nullptr;
    bool ownsStore_ = false;
    bool initialized_ = false;

    std::array<String, AppConfig::MAX_NOTES> notes_;
    size_t head_ = 0;
    size_t count_ = 0;
    uint8_t activeBank_ = 0;
    uint32_t generation_ = 0;
    String lastError_;
    String recoveryDiagnostic_;
};
