/**
 * @file TimeService.h
 * @brief Gestion du temps RTC, NTP non bloquant, UTC et fuseau Europe/Zurich — VoiceJournal v0.3.0
 */

#pragma once

#include <cstdint>
#include <ctime>
#include <string>

namespace VoiceJournal {

struct DateRange {
    int64_t startUtc = 0;
    int64_t endUtc = 0;
    bool isValid = false;
};

class TimeService {
public:
    static constexpr const char* TZ_ZURICH = "CET-1CEST,M3.5.0,M10.5.0/3";

    TimeService();

    /**
     * @brief Initialise le fuseau horaire et démarre la synchronisation NTP si Wi-Fi connecté.
     */
    void begin(bool startNtp = true);

    /**
     * @brief Appel périodique non bloquant pour vérifier l'état NTP et RTC.
     */
    void update();

    /**
     * @brief Indique si l'heure est synchronisée et fiable (RTC ou NTP valide >= 2024).
     */
    bool isTimeReliable() const;

    /**
     * @brief Retourne l'heure UTC actuelle en secondes epoch (0 si non fiable).
     */
    int64_t getCurrentUtc() const;

    /**
     * @brief Formate une heure locale pour l'affichage (ex: "14:38").
     */
    std::string formatLocalTime(int64_t utcEpoch = 0) const;

    /**
     * @brief Formate une date locale pour l'affichage (ex: "Aujourd'hui", "Hier", "25 nov.").
     */
    std::string formatDisplayDate(int64_t utcEpoch) const;

    /**
     * @brief Formate une date et heure complète ISO-8601 Zurich (ex: "2026-11-25T14:30:00+01:00").
     */
    std::string formatIso8601Zurich(int64_t utcEpoch) const;

    /**
     * @brief Analyse une expression temporelle relative en français et retourne la plage UTC.
     * @param expr "aujourd'hui", "hier", "demain", "ce matin", "cette semaine", "la semaine dernière"
     */
    DateRange parseRelativeDateFrench(const std::string& expr) const;

    // Pour simulation et tests natifs
    void setMockTime(int64_t simulatedUtc);
    void clearMockTime();

private:
    bool ntpStarted_ = false;
    bool ntpSynced_ = false;
    int64_t mockTime_ = -1;
    uint32_t lastNtpCheckMs_ = 0;

    void syncRtcToSystem();
    void syncSystemToRtc();
};

} // namespace VoiceJournal
