/**
 * @file TimeService.cpp
 * @brief Implémentation du service horaire UTC / Europe/Zurich — VoiceJournal v0.3.0
 */

#include "TimeService.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

#ifdef ARDUINO
#include <Arduino.h>
#include <M5Unified.h>
#include <time.h>
#include <sys/time.h>
#endif

namespace VoiceJournal {

TimeService::TimeService() = default;

void TimeService::begin(bool startNtp) {
#if defined(_WIN32)
    _putenv("TZ=CET-1CEST,M3.5.0,M10.5.0/3");
    _tzset();
#elif defined(ARDUINO)
    setenv("TZ", TZ_ZURICH, 1);
    tzset();
    if (startNtp) {
        configTzTime(TZ_ZURICH, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
        ntpStarted_ = true;
    }
    syncRtcToSystem();
#else
    setenv("TZ", TZ_ZURICH, 1);
    tzset();
#endif
}

void TimeService::update() {
#ifdef ARDUINO
    uint32_t nowMs = millis();
    if (!ntpSynced_ && (nowMs - lastNtpCheckMs_ > 5000)) {
        lastNtpCheckMs_ = nowMs;
        time_t now = 0;
        std::time(&now);
        // Année 2024 = ~1704067200 (heure synchronisée par VoiceKit ou SNTP)
        if (now > 1704067200) {
            // Réappliquer le fuseau Europe/Zurich (VoiceKit appelle configTime(0, 0, ...) en UTC)
            setenv("TZ", TZ_ZURICH, 1);
            tzset();
            ntpSynced_ = true;
            syncSystemToRtc();
        }
    }
#endif
}

void TimeService::syncRtcToSystem() {
#ifdef ARDUINO
    if (M5.Rtc.isEnabled()) {
        auto dt = M5.Rtc.getDateTime();
        struct tm tmVal;
        std::memset(&tmVal, 0, sizeof(tmVal));
        tmVal.tm_year = dt.date.year - 1900;
        tmVal.tm_mon  = dt.date.month - 1;
        tmVal.tm_mday = dt.date.date;
        tmVal.tm_hour = dt.time.hours;
        tmVal.tm_min  = dt.time.minutes;
        tmVal.tm_sec  = dt.time.seconds;

        time_t t = mktime(&tmVal);
        if (t > 1704067200) {
            struct timeval tv = { t, 0 };
            settimeofday(&tv, nullptr);
        }
    }
#endif
}

void TimeService::syncSystemToRtc() {
#ifdef ARDUINO
    if (M5.Rtc.isEnabled()) {
        time_t now = 0;
        std::time(&now);
        struct tm* tmVal = localtime(&now);
        if (tmVal) {
            m5::rtc_datetime_t dt;
            dt.date.year  = tmVal->tm_year + 1900;
            dt.date.month = tmVal->tm_mon + 1;
            dt.date.date  = tmVal->tm_mday;
            dt.time.hours = tmVal->tm_hour;
            dt.time.minutes = tmVal->tm_min;
            dt.time.seconds = tmVal->tm_sec;
            M5.Rtc.setDateTime(dt);
        }
    }
#endif
}

bool TimeService::isTimeReliable() const {
    if (mockTime_ >= 0) return mockTime_ > 1704067200;
    time_t now = 0;
    std::time(&now);
    return now > 1704067200; // >= 1er janvier 2024
}

int64_t TimeService::getCurrentUtc() const {
    if (mockTime_ >= 0) return mockTime_;
    if (!isTimeReliable()) return 0;
    time_t now = 0;
    std::time(&now);
    return static_cast<int64_t>(now);
}

std::string TimeService::formatLocalTime(int64_t utcEpoch) const {
    if (utcEpoch == 0) {
        utcEpoch = getCurrentUtc();
    }
    if (utcEpoch == 0) return "--:--";

    time_t t = static_cast<time_t>(utcEpoch);
    struct tm* tmVal = std::localtime(&t);
    if (!tmVal) return "--:--";

    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", tmVal->tm_hour, tmVal->tm_min);
    return std::string(buf);
}

std::string TimeService::formatDisplayDate(int64_t utcEpoch) const {
    if (utcEpoch == 0) return "Inconnue";

    int64_t nowUtc = getCurrentUtc();
    if (nowUtc == 0) {
        // Si l'heure courante n'est pas connue mais la date est présente
        time_t t = static_cast<time_t>(utcEpoch);
        struct tm* tmVal = std::localtime(&t);
        if (!tmVal) return "Inconnue";
        char buf[32];
        static const char* const MONTHS[] = {"janv.", "févr.", "mars", "avr.", "mai", "juin", "juil.", "août", "sept.", "oct.", "nov.", "déc."};
        std::snprintf(buf, sizeof(buf), "%d %s", tmVal->tm_mday, MONTHS[tmVal->tm_mon]);
        return std::string(buf);
    }

    time_t tNow = static_cast<time_t>(nowUtc);
    time_t tEntry = static_cast<time_t>(utcEpoch);
    struct tm tmNow = *std::localtime(&tNow);
    struct tm tmEntry = *std::localtime(&tEntry);

    // Calcul du début de journée locale pour aujourd'hui et pour l'entrée
    tmNow.tm_hour = 0; tmNow.tm_min = 0; tmNow.tm_sec = 0;
    time_t startToday = std::mktime(&tmNow);

    tmEntry.tm_hour = 0; tmEntry.tm_min = 0; tmEntry.tm_sec = 0;
    time_t startEntryDay = std::mktime(&tmEntry);

    double diffDays = std::difftime(startToday, startEntryDay) / 86400.0;
    if (diffDays >= -0.5 && diffDays < 0.5) {
        return "Aujourd'hui";
    } else if (diffDays >= 0.5 && diffDays < 1.5) {
        return "Hier";
    } else if (diffDays >= -1.5 && diffDays < -0.5) {
        return "Demain";
    }

    struct tm* tmShow = std::localtime(&tEntry);
    char buf[32];
    static const char* const MONTHS[] = {"janv.", "févr.", "mars", "avr.", "mai", "juin", "juil.", "août", "sept.", "oct.", "nov.", "déc."};
    std::snprintf(buf, sizeof(buf), "%d %s", tmShow->tm_mday, MONTHS[tmShow->tm_mon]);
    return std::string(buf);
}

std::string TimeService::formatIso8601Zurich(int64_t utcEpoch) const {
    if (utcEpoch == 0) return "";
    time_t t = static_cast<time_t>(utcEpoch);
    struct tm* tmVal = std::localtime(&t);
    if (!tmVal) return "";

    // Calcul de l'offset UTC local
    struct tm gm = *std::gmtime(&t);
    time_t tGm = std::mktime(&gm);
    int offsetSeconds = static_cast<int>(std::difftime(t, tGm));
    int offsetHours = offsetSeconds / 3600;
    int offsetMinutes = std::abs((offsetSeconds % 3600) / 60);

    char buf[48];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d%+03d:%02d",
                  tmVal->tm_year + 1900, tmVal->tm_mon + 1, tmVal->tm_mday,
                  tmVal->tm_hour, tmVal->tm_min, tmVal->tm_sec,
                  offsetHours, offsetMinutes);
    return std::string(buf);
}

DateRange TimeService::parseRelativeDateFrench(const std::string& expr) const {
    DateRange r;
    int64_t nowUtc = getCurrentUtc();
    if (nowUtc == 0) return r; // Heure non fiable

    time_t tNow = static_cast<time_t>(nowUtc);
    struct tm tmNow = *std::localtime(&tNow);

    // Début de la journée courante locale (00:00:00)
    tmNow.tm_hour = 0; tmNow.tm_min = 0; tmNow.tm_sec = 0;
    time_t startToday = std::mktime(&tmNow);

    if (expr == "aujourd'hui" || expr == "aujourd hui") {
        r.startUtc = startToday;
        r.endUtc = startToday + 86399;
        r.isValid = true;
    } else if (expr == "hier") {
        r.startUtc = startToday - 86400;
        r.endUtc = startToday - 1;
        r.isValid = true;
    } else if (expr == "demain") {
        r.startUtc = startToday + 86400;
        r.endUtc = startToday + 86400 + 86399;
        r.isValid = true;
    } else if (expr == "ce matin") {
        r.startUtc = startToday + (6 * 3600); // 06:00
        r.endUtc = startToday + (12 * 3600);  // 12:00
        r.isValid = true;
    } else if (expr == "cette semaine") {
        // En France/Suisse, la semaine commence le lundi (tm_wday = 1, dimanche = 0)
        int dayOfWeek = tmNow.tm_wday; // 0 = Dimanche, 1 = Lundi, ..., 6 = Samedi
        int daysSinceMonday = (dayOfWeek == 0) ? 6 : (dayOfWeek - 1);
        time_t startWeek = startToday - (daysSinceMonday * 86400);
        r.startUtc = startWeek;
        r.endUtc = startWeek + (7 * 86400) - 1;
        r.isValid = true;
    } else if (expr == "la semaine dernière" || expr == "semaine derniere") {
        int dayOfWeek = tmNow.tm_wday;
        int daysSinceMonday = (dayOfWeek == 0) ? 6 : (dayOfWeek - 1);
        time_t startLastWeek = startToday - ((daysSinceMonday + 7) * 86400);
        r.startUtc = startLastWeek;
        r.endUtc = startLastWeek + (7 * 86400) - 1;
        r.isValid = true;
    }

    return r;
}

void TimeService::setMockTime(int64_t simulatedUtc) {
    mockTime_ = simulatedUtc;
}

void TimeService::clearMockTime() {
    mockTime_ = -1;
}

} // namespace VoiceJournal
