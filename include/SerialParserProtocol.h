#pragma once

#include <string>
#include <sstream>
#include <cstdint>
#include "parser/ParserTypes.h"
#include "CommandRouter.h"
#include "MemoryModel.h"

namespace VoiceJournal::Protocol {

inline std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<uint8_t>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<uint8_t>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

inline std::string formatParseJson(const Parser::ParseResult& pr) {
    std::ostringstream ss;
    ss << "{\n"
       << "  \"command\": \"PARSE\",\n"
       << "  \"input\": \"" << escapeJson(pr.originalText) << "\",\n"
       << "  \"intent\": \"" << Parser::intentToString(pr.intent) << "\",\n"
       << "  \"status\": \"" << Parser::parseStatusToString(pr.status) << "\",\n"
       << "  \"action\": \"" << escapeJson(pr.semanticFrame.action) << "\",\n"
       << "  \"entity\": \"" << escapeJson(pr.semanticFrame.entity) << "\",\n"
       << "  \"relativeDate\": \"" << escapeJson(pr.semanticFrame.relativeDate) << "\",\n"
       << "  \"executable\": " << (pr.executable ? "true" : "false") << "\n"
       << "}";
    return ss.str();
}

inline std::string formatParseShadowJson(const CommandResult& res, const std::string& input) {
    std::ostringstream ss;
    ss << "{\n"
       << "  \"command\": \"PARSE_SHADOW\",\n"
       << "  \"input\": \"" << escapeJson(input) << "\",\n"
       << "  \"legacy\": \"" << commandTypeToString(res.legacyType) << "\",\n"
       << "  \"intelligent\": \"" << Parser::intentToString(res.parseResult.intent) << "\",\n"
       << "  \"comparisonStatus\": \"" << res.comparisonStatus.c_str() << "\",\n"
       << "  \"blocked\": " << (res.blockedByDivergence ? "true" : "false") << "\n"
       << "}";
    return ss.str();
}

inline std::string formatParserStatusJson(
    const char* version,
    const char* modeLabel,
    uint32_t freeHeap,
    uint32_t maxAlloc,
    uint32_t freePsram,
    size_t rulesCount,
    size_t phrasesCount
) {
    std::ostringstream ss;
    ss << "{\n"
       << "  \"command\": \"PARSER_STATUS\",\n"
       << "  \"version\": \"" << version << "\",\n"
       << "  \"mode\": \"" << modeLabel << "\",\n"
       << "  \"freeHeap\": " << freeHeap << ",\n"
       << "  \"maxAllocHeap\": " << maxAlloc << ",\n"
       << "  \"freePsram\": " << freePsram << ",\n"
       << "  \"rulesCount\": " << rulesCount << ",\n"
       << "  \"phrasesCount\": " << phrasesCount << "\n"
       << "}";
    return ss.str();
}

inline std::string formatJournalStatusJson(
    const char* version,
    size_t entryCount,
    size_t capacity,
    size_t projectCount,
    size_t openTasksCount,
    size_t usedBytes,
    size_t totalBytes,
    bool timeReliable,
    const std::string& currentProject
) {
    std::ostringstream ss;
    ss << "{\n"
       << "  \"command\": \"JOURNAL_STATUS\",\n"
       << "  \"version\": \"" << version << "\",\n"
       << "  \"entryCount\": " << entryCount << ",\n"
       << "  \"capacity\": " << capacity << ",\n"
       << "  \"projectCount\": " << projectCount << ",\n"
       << "  \"openTasksCount\": " << openTasksCount << ",\n"
       << "  \"storageUsedBytes\": " << usedBytes << ",\n"
       << "  \"storageTotalBytes\": " << totalBytes << ",\n"
       << "  \"timeReliable\": " << (timeReliable ? "true" : "false") << ",\n"
       << "  \"currentProject\": \"" << escapeJson(currentProject) << "\"\n"
       << "}";
    return ss.str();
}

inline std::string formatEntryJson(const JournalEntry& entry) {
    std::ostringstream ss;
    ss << "{\n"
       << "  \"id\": " << entry.id << ",\n"
       << "  \"type\": \"" << entryTypeToString(entry.type) << "\",\n"
       << "  \"projectId\": " << entry.projectId << ",\n"
       << "  \"createdAt\": " << entry.createdAt << ",\n"
       << "  \"updatedAt\": " << entry.updatedAt << ",\n"
       << "  \"status\": \"" << entryStatusToString(entry.status) << "\",\n"
       << "  \"dueAt\": " << entry.dueAt << ",\n"
       << "  \"text\": \"" << escapeJson(entry.text) << "\"\n"
       << "}";
    return ss.str();
}

inline std::string formatProjectJson(const Project& project) {
    std::ostringstream ss;
    ss << "{\n"
       << "  \"id\": " << project.id << ",\n"
       << "  \"name\": \"" << escapeJson(project.name) << "\",\n"
       << "  \"createdAt\": " << project.createdAt << ",\n"
       << "  \"status\": \"" << projectStatusToString(project.status) << "\"\n"
       << "}";
    return ss.str();
}

inline std::string formatParserHelpJson() {
    return "{\n"
           "  \"command\": \"PARSER_HELP\",\n"
           "  \"commands\": [\n"
           "    \"PARSE <texte>\",\n"
           "    \"PARSE_SHADOW <texte>\",\n"
           "    \"PARSER_STATUS\",\n"
           "    \"PARSER_HELP\",\n"
           "    \"JOURNAL_STATUS\",\n"
           "    \"LIST_ENTRIES [offset] [limit]\",\n"
           "    \"GET_ENTRY <id>\",\n"
           "    \"LIST_PROJECTS\",\n"
           "    \"LIST_TASKS\",\n"
           "    \"EXPORT_JOURNAL\"\n"
           "  ]\n"
           "}";
}

inline std::string formatErrorJson(const std::string& errorStatus, const std::string& message) {
    std::ostringstream ss;
    ss << "{\n"
       << "  \"command\": \"ERROR\",\n"
       << "  \"status\": \"" << escapeJson(errorStatus) << "\",\n"
       << "  \"message\": \"" << escapeJson(message) << "\"\n"
       << "}";
    return ss.str();
}

} // namespace VoiceJournal::Protocol
