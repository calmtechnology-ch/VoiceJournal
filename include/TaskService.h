/**
 * @file TaskService.h
 * @brief Gestionnaire de tâches — VoiceJournal v0.3.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "JournalRepository.h"
#include "TimeService.h"

namespace VoiceJournal {

class TaskService {
public:
    TaskService(JournalRepository& repo, TimeService& timeService);

    /**
     * @brief Crée une nouvelle tâche.
     */
    bool addTask(const std::string& text, uint32_t projectId, int64_t dueAt, uint32_t& outId);

    /**
     * @brief Marque une tâche comme terminée.
     */
    bool completeTask(uint32_t id);

    /**
     * @brief Rouvre une tâche terminée ou annulée.
     */
    bool reopenTask(uint32_t id);

    /**
     * @brief Annule une tâche.
     */
    bool cancelTask(uint32_t id);

    /**
     * @brief Liste les tâches selon critères.
     */
    std::vector<JournalEntry> listTasks(bool openOnly = true, int32_t projectId = -1, size_t limit = 100) const;

    /**
     * @brief Recherche des tâches par mot-clé dans la description.
     */
    std::vector<JournalEntry> findTasks(const std::string& query, bool openOnly = true) const;

    /**
     * @brief Compte les tâches ouvertes (à faire).
     */
    size_t countOpenTasks(int32_t projectId = -1) const;

private:
    JournalRepository& repo_;
    TimeService& timeService_;
};

} // namespace VoiceJournal
