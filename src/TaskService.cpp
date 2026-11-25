/**
 * @file TaskService.cpp
 * @brief Implémentation du service de gestion des tâches — VoiceJournal v0.3.0
 */

#include "TaskService.h"
#include "parser/TextNormalizer.h"

namespace VoiceJournal {

TaskService::TaskService(JournalRepository& repo, TimeService& timeService)
    : repo_(repo), timeService_(timeService) {
}

bool TaskService::addTask(const std::string& text, uint32_t projectId, int64_t dueAt, uint32_t& outId) {
    if (text.empty()) return false;

    JournalEntry entry;
    entry.type = EntryType::TASK;
    entry.projectId = projectId;
    entry.createdAt = timeService_.getCurrentUtc();
    entry.updatedAt = 0;
    entry.status = EntryStatus::OPEN;
    entry.dueAt = dueAt;
    entry.text = text;

    if (!repo_.addEntry(entry)) {
        return false;
    }

    outId = entry.id;
    return true;
}

bool TaskService::completeTask(uint32_t id) {
    JournalEntry entry;
    if (!repo_.getEntry(id, entry)) return false;
    if (entry.type != EntryType::TASK) return false;

    entry.status = EntryStatus::COMPLETED;
    entry.updatedAt = timeService_.getCurrentUtc();
    return repo_.updateEntry(entry);
}

bool TaskService::reopenTask(uint32_t id) {
    JournalEntry entry;
    if (!repo_.getEntry(id, entry)) return false;
    if (entry.type != EntryType::TASK) return false;

    entry.status = EntryStatus::OPEN;
    entry.updatedAt = timeService_.getCurrentUtc();
    return repo_.updateEntry(entry);
}

bool TaskService::cancelTask(uint32_t id) {
    JournalEntry entry;
    if (!repo_.getEntry(id, entry)) return false;
    if (entry.type != EntryType::TASK) return false;

    entry.status = EntryStatus::CANCELLED;
    entry.updatedAt = timeService_.getCurrentUtc();
    return repo_.updateEntry(entry);
}

std::vector<JournalEntry> TaskService::listTasks(bool openOnly, int32_t projectId, size_t limit) const {
    EntryFilter filter;
    filter.type = static_cast<int16_t>(EntryType::TASK);
    filter.projectId = projectId;
    if (openOnly) {
        filter.status = static_cast<int16_t>(EntryStatus::OPEN);
    }

    std::vector<uint32_t> ids = repo_.queryEntryIds(filter, limit);
    std::vector<JournalEntry> tasks;
    tasks.reserve(ids.size());

    for (uint32_t id : ids) {
        JournalEntry e;
        if (repo_.getEntry(id, e)) {
            tasks.push_back(e);
        }
    }
    return tasks;
}

std::vector<JournalEntry> TaskService::findTasks(const std::string& query, bool openOnly) const {
    std::vector<JournalEntry> allTasks = listTasks(openOnly, -1, 100);
    if (query.empty()) return allTasks;

    std::string normQuery = Parser::TextNormalizer::normalizeSimple(query);
    std::vector<JournalEntry> matches;

    for (const auto& t : allTasks) {
        std::string normText = Parser::TextNormalizer::normalizeSimple(t.text);
        if (normText.find(normQuery) != std::string::npos) {
            matches.push_back(t);
        }
    }
    return matches;
}

size_t TaskService::countOpenTasks(int32_t projectId) const {
    EntryFilter filter;
    filter.type = static_cast<int16_t>(EntryType::TASK);
    filter.status = static_cast<int16_t>(EntryStatus::OPEN);
    filter.projectId = projectId;
    return repo_.queryEntryIds(filter).size();
}

} // namespace VoiceJournal
