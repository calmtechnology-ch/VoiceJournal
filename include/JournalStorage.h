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
 * Strategie :
 * - Tampon circulaire de AppConfig::MAX_NOTES (5) emplacements ("n0".."n4").
 * - Metadonnees "magic", "head" (index de la plus ancienne note) et "count" (0..5).
 * - Ecriture de la charge utile avant mise a jour des metadonnees pour limiter
 *   les risques de corruption lors d'une coupure d'alimentation.
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
    static constexpr uint32_t kStorageMagic = 0x564A3031UL; // "VJ01"
    static constexpr const char* kKeyMagic  = "magic";
    static constexpr const char* kKeyHead   = "head";
    static constexpr const char* kKeyCount  = "count";

    static void formatSlotKey(size_t slot, char* outBuf, size_t bufSize);

    IKeyValueStore* store_ = nullptr;
    bool ownsStore_ = false;
    bool initialized_ = false;

    std::array<String, AppConfig::MAX_NOTES> notes_;
    size_t head_ = 0;
    size_t count_ = 0;
    String lastError_;
};
