/**
 * @file JournalRepository.cpp
 * @brief Implémentation du dépôt LittleFS / natif append-only avec index RAM — VoiceJournal v0.3.0
 */

#include "JournalRepository.h"
#include <sstream>
#include <algorithm>
#include <cstring>

namespace VoiceJournal {

namespace {

static constexpr uint8_t RECORD_MAGIC0 = 0xAA;
static constexpr uint8_t RECORD_MAGIC1 = 0x55;
static constexpr uint8_t TYPE_ENTRY_DATA = 1;
static constexpr uint8_t TYPE_ENTRY_DELETE = 2;
static constexpr uint8_t TYPE_PROJECT_DATA = 10;

// En-tête du fichier journal (8 octets)
static const char FILE_HEADER[8] = {'V', 'J', '0', '3', 'L', 'O', 'G', '1'};

// Sérialisation JSON d'une chaîne avec échappement
std::string escapeJsonString(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 16);
    for (char c : input) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<uint8_t>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<uint8_t>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

// Extraction simple d'une clé-valeur JSON
bool findJsonInt(const std::string& json, const std::string& key, int64_t& outVal) {
    std::string needle = "\"" + key + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return false;
    pos += needle.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    char* endPtr = nullptr;
    outVal = std::strtoll(json.c_str() + pos, &endPtr, 10);
    return endPtr != (json.c_str() + pos);
}

bool findJsonString(const std::string& json, const std::string& key, std::string& outVal) {
    std::string needle = "\"" + key + "\":\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return false;
    pos += needle.size();
    outVal.clear();
    bool escaped = false;
    while (pos < json.size()) {
        char c = json[pos++];
        if (escaped) {
            switch (c) {
                case '"': outVal += '"'; break;
                case '\\': outVal += '\\'; break;
                case 'n': outVal += '\n'; break;
                case 'r': outVal += '\r'; break;
                case 't': outVal += '\t'; break;
                default: outVal += c; break;
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            return true;
        } else {
            outVal += c;
        }
    }
    return false;
}

bool findJsonArray(const std::string& json, const std::string& key, std::vector<std::string>& outList) {
    outList.clear();
    std::string needle = "\"" + key + "\":[";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return false;
    pos += needle.size();

    while (pos < json.size() && json[pos] != ']') {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == ',' || json[pos] == '\t' || json[pos] == '\n')) pos++;
        if (pos < json.size() && json[pos] == '"') {
            pos++;
            std::string item;
            bool escaped = false;
            while (pos < json.size()) {
                char c = json[pos++];
                if (escaped) {
                    item += c;
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                } else if (c == '"') {
                    outList.push_back(item);
                    break;
                } else {
                    item += c;
                }
            }
        } else {
            pos++;
        }
    }
    return true;
}

} // namespace

uint32_t JournalRepository::computeCrc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

std::string JournalRepository::serializeEntry(const JournalEntry& entry) {
    std::ostringstream ss;
    ss << "{\"id\":" << entry.id
       << ",\"type\":" << static_cast<int>(entry.type)
       << ",\"proj\":" << entry.projectId
       << ",\"cat\":" << entry.createdAt
       << ",\"uat\":" << entry.updatedAt
       << ",\"st\":" << static_cast<int>(entry.status)
       << ",\"dat\":" << entry.dueAt
       << ",\"text\":\"" << escapeJsonString(entry.text) << "\"";

    ss << ",\"ent\":[";
    for (size_t i = 0; i < entry.entities.size(); i++) {
        if (i > 0) ss << ",";
        ss << "\"" << escapeJsonString(entry.entities[i]) << "\"";
    }
    ss << "]";

    ss << ",\"tags\":[";
    for (size_t i = 0; i < entry.tags.size(); i++) {
        if (i > 0) ss << ",";
        ss << "\"" << escapeJsonString(entry.tags[i]) << "\"";
    }
    ss << "]}";

    return ss.str();
}

bool JournalRepository::deserializeEntry(const std::string& json, JournalEntry& entry) {
    int64_t val = 0;
    if (findJsonInt(json, "id", val)) entry.id = static_cast<uint32_t>(val);
    else return false;

    if (findJsonInt(json, "type", val)) entry.type = static_cast<EntryType>(val);
    if (findJsonInt(json, "proj", val)) entry.projectId = static_cast<uint32_t>(val);
    if (findJsonInt(json, "cat", val)) entry.createdAt = val;
    if (findJsonInt(json, "uat", val)) entry.updatedAt = val;
    if (findJsonInt(json, "st", val)) entry.status = static_cast<EntryStatus>(val);
    if (findJsonInt(json, "dat", val)) entry.dueAt = val;

    findJsonString(json, "text", entry.text);
    findJsonArray(json, "ent", entry.entities);
    findJsonArray(json, "tags", entry.tags);

    return true;
}

std::string JournalRepository::serializeProject(const Project& project) {
    std::ostringstream ss;
    ss << "{\"id\":" << project.id
       << ",\"name\":\"" << escapeJsonString(project.name) << "\""
       << ",\"cat\":" << project.createdAt
       << ",\"st\":" << static_cast<int>(project.status)
       << ",\"alias\":[";
    for (size_t i = 0; i < project.aliases.size(); i++) {
        if (i > 0) ss << ",";
        ss << "\"" << escapeJsonString(project.aliases[i]) << "\"";
    }
    ss << "]}";
    return ss.str();
}

bool JournalRepository::deserializeProject(const std::string& json, Project& project) {
    int64_t val = 0;
    if (findJsonInt(json, "id", val)) project.id = static_cast<uint32_t>(val);
    else return false;

    findJsonString(json, "name", project.name);
    if (findJsonInt(json, "cat", val)) project.createdAt = val;
    if (findJsonInt(json, "st", val)) project.status = static_cast<ProjectStatus>(val);
    findJsonArray(json, "alias", project.aliases);
    return true;
}

JournalRepository::JournalRepository(IFileSystem* fs)
    : fs_(fs) {
}

JournalRepository::~JournalRepository() = default;

bool JournalRepository::begin() {
    if (!fs_) {
        lastError_ = "FS_NULL";
        return false;
    }

    entriesIndex_.clear();
    entryIdToIdx_.clear();
    projectsIndex_.clear();
    projectIdToIdx_.clear();
    nextEntryId_ = 1;
    nextProjectId_ = 1;

    // Si le fichier journal n'existe pas, le créer avec l'en-tête
    if (!fs_->exists(LOG_PATH)) {
        IFileHandle* file = fs_->open(LOG_PATH, "w");
        if (!file) {
            lastError_ = "CREATE_LOG_FAILED";
            return false;
        }
        file->write(reinterpret_cast<const uint8_t*>(FILE_HEADER), sizeof(FILE_HEADER));
        file->flush();
        file->close();
        delete file;
        initialized_ = true;
        return true;
    }

    // Ouvrir et scanner les enregistrements existants
    IFileHandle* file = fs_->open(LOG_PATH, "r+");
    if (!file) {
        file = fs_->open(LOG_PATH, "r");
    }
    if (!file) {
        lastError_ = "OPEN_LOG_FAILED";
        return false;
    }

    size_t fileSize = file->size();
    if (fileSize < sizeof(FILE_HEADER)) {
        // Fichier trop court ou vide : réinitialiser l'en-tête
        file->seek(0);
        file->write(reinterpret_cast<const uint8_t*>(FILE_HEADER), sizeof(FILE_HEADER));
        file->flush();
        file->close();
        delete file;
        initialized_ = true;
        return true;
    }

    // Vérifier l'en-tête
    char hdr[8];
    file->read(reinterpret_cast<uint8_t*>(hdr), sizeof(hdr));
    if (std::memcmp(hdr, FILE_HEADER, sizeof(FILE_HEADER)) != 0) {
        file->close();
        delete file;
        lastError_ = "INVALID_HEADER";
        return false;
    }

    // Scanner les enregistrements
    size_t lastValidPos = sizeof(FILE_HEADER);
    while (lastValidPos + 12 <= fileSize) {
        file->seek(lastValidPos);
        uint8_t m0 = 0, m1 = 0, recType = 0;
        file->read(&m0, 1);
        file->read(&m1, 1);
        file->read(&recType, 1);

        if (m0 != RECORD_MAGIC0 || m1 != RECORD_MAGIC1) {
            // Fin des enregistrements valides ou corruption
            break;
        }

        uint32_t payloadLen = 0;
        file->read(reinterpret_cast<uint8_t*>(&payloadLen), 4);

        if (payloadLen > 4096 || lastValidPos + 2 + 1 + 4 + payloadLen + 4 + 1 > fileSize) {
            // Enregistrement incomplet (coupure d'alimentation en cours d'écriture)
            break;
        }

        std::string payload;
        payload.resize(payloadLen);
        file->read(reinterpret_cast<uint8_t*>(&payload[0]), payloadLen);

        uint32_t expectedCrc = 0;
        file->read(reinterpret_cast<uint8_t*>(&expectedCrc), 4);
        uint8_t trailingNl = 0;
        file->read(&trailingNl, 1);

        uint32_t actualCrc = computeCrc32(reinterpret_cast<const uint8_t*>(payload.data()), payloadLen);
        if (actualCrc != expectedCrc || trailingNl != '\n') {
            // CRC invalide : enregistrement altéré
            break;
        }

        uint32_t payloadOffset = static_cast<uint32_t>(lastValidPos + 2 + 1 + 4);

        // Enregistrement valide : mettre à jour l'index
        if (recType == TYPE_ENTRY_DATA) {
            JournalEntry entry;
            if (deserializeEntry(payload, entry)) {
                if (entry.id >= nextEntryId_) nextEntryId_ = entry.id + 1;

                auto it = entryIdToIdx_.find(entry.id);
                if (it != entryIdToIdx_.end()) {
                    // Mise à jour d'une entrée existante
                    IndexEntry& ie = entriesIndex_[it->second];
                    ie.fileOffset = payloadOffset;
                    ie.payloadLength = payloadLen;
                    ie.type = entry.type;
                    ie.status = entry.status;
                    ie.projectId = entry.projectId;
                    ie.createdAt = entry.createdAt;
                    ie.dueAt = entry.dueAt;
                    ie.isDeleted = false;
                } else {
                    // Nouvelle entrée
                    IndexEntry ie;
                    ie.id = entry.id;
                    ie.fileOffset = payloadOffset;
                    ie.payloadLength = payloadLen;
                    ie.type = entry.type;
                    ie.status = entry.status;
                    ie.projectId = entry.projectId;
                    ie.createdAt = entry.createdAt;
                    ie.dueAt = entry.dueAt;
                    ie.isDeleted = false;

                    entryIdToIdx_[entry.id] = entriesIndex_.size();
                    entriesIndex_.push_back(ie);
                }
            }
        } else if (recType == TYPE_ENTRY_DELETE) {
            int64_t delId = 0;
            if (findJsonInt(payload, "id", delId)) {
                auto it = entryIdToIdx_.find(static_cast<uint32_t>(delId));
                if (it != entryIdToIdx_.end()) {
                    entriesIndex_[it->second].isDeleted = true;
                }
            }
        } else if (recType == TYPE_PROJECT_DATA) {
            Project proj;
            if (deserializeProject(payload, proj)) {
                if (proj.id >= nextProjectId_) nextProjectId_ = proj.id + 1;

                auto it = projectIdToIdx_.find(proj.id);
                if (it != projectIdToIdx_.end()) {
                    IndexProject& ip = projectsIndex_[it->second];
                    ip.fileOffset = payloadOffset;
                    ip.payloadLength = payloadLen;
                    ip.status = proj.status;
                    ip.name = proj.name;
                } else {
                    IndexProject ip;
                    ip.id = proj.id;
                    ip.fileOffset = payloadOffset;
                    ip.payloadLength = payloadLen;
                    ip.status = proj.status;
                    ip.name = proj.name;

                    projectIdToIdx_[proj.id] = projectsIndex_.size();
                    projectsIndex_.push_back(ip);
                }
            }
        }

        lastValidPos += (2 + 1 + 4 + payloadLen + 4 + 1);
    }

    // Si le fichier contenait des octets incomplets en fin de fichier, le tronquer au dernier enregistrement valide
    if (lastValidPos < fileSize) {
        file->truncate(lastValidPos);
    }

    file->close();
    delete file;

    initialized_ = true;
    return true;
}

bool JournalRepository::appendRecord(uint8_t recordType, const std::string& payload, uint32_t& outOffset, uint32_t& outLen) {
    if (!fs_) return false;
    IFileHandle* file = fs_->open(LOG_PATH, "a");
    if (!file) {
        lastError_ = "APPEND_OPEN_FAILED";
        return false;
    }

    size_t curPos = file->size();
    file->seek(curPos);

    uint8_t header[7];
    header[0] = RECORD_MAGIC0;
    header[1] = RECORD_MAGIC1;
    header[2] = recordType;
    uint32_t pLen = static_cast<uint32_t>(payload.size());
    std::memcpy(&header[3], &pLen, 4);

    if (file->write(header, sizeof(header)) != sizeof(header)) {
        file->close();
        delete file;
        lastError_ = "WRITE_HDR_FAILED";
        return false;
    }

    outOffset = static_cast<uint32_t>(curPos + sizeof(header));
    outLen = pLen;

    if (pLen > 0) {
        if (file->write(reinterpret_cast<const uint8_t*>(payload.data()), pLen) != pLen) {
            file->close();
            delete file;
            lastError_ = "WRITE_PAYLOAD_FAILED";
            return false;
        }
    }

    uint32_t crc = computeCrc32(reinterpret_cast<const uint8_t*>(payload.data()), pLen);
    if (file->write(reinterpret_cast<const uint8_t*>(&crc), 4) != 4) {
        file->close();
        delete file;
        lastError_ = "WRITE_CRC_FAILED";
        return false;
    }

    uint8_t nl = '\n';
    file->write(&nl, 1);
    file->flush();
    file->close();
    delete file;
    return true;
}

bool JournalRepository::addEntry(JournalEntry& entry) {
    if (!initialized_) return false;
    if (countEntries() >= MAX_CAPACITY) {
        lastError_ = "STORAGE_FULL";
        return false;
    }

    if (entry.status == EntryStatus::NONE) {
        entry.status = defaultStatusForType(entry.type);
    }
    entry.id = nextEntryId_++;
    if (entry.status == EntryStatus::NONE) {
        entry.status = defaultStatusForType(entry.type);
    }
    std::string payload = serializeEntry(entry);
    uint32_t offset = 0, len = 0;
    if (!appendRecord(TYPE_ENTRY_DATA, payload, offset, len)) {
        return false;
    }

    IndexEntry ie;
    ie.id = entry.id;
    ie.fileOffset = offset;
    ie.payloadLength = len;
    ie.type = entry.type;
    ie.status = entry.status;
    ie.projectId = entry.projectId;
    ie.createdAt = entry.createdAt;
    ie.dueAt = entry.dueAt;
    ie.isDeleted = false;

    entryIdToIdx_[entry.id] = entriesIndex_.size();
    entriesIndex_.push_back(ie);
    return true;
}

bool JournalRepository::updateEntry(const JournalEntry& entry) {
    if (!initialized_) return false;
    auto it = entryIdToIdx_.find(entry.id);
    if (it == entryIdToIdx_.end() || entriesIndex_[it->second].isDeleted) {
        lastError_ = "ENTRY_NOT_FOUND";
        return false;
    }

    std::string payload = serializeEntry(entry);
    uint32_t offset = 0, len = 0;
    if (!appendRecord(TYPE_ENTRY_DATA, payload, offset, len)) {
        return false;
    }

    IndexEntry& ie = entriesIndex_[it->second];
    ie.fileOffset = offset;
    ie.payloadLength = len;
    ie.type = entry.type;
    ie.status = entry.status;
    ie.projectId = entry.projectId;
    ie.createdAt = entry.createdAt;
    ie.dueAt = entry.dueAt;
    return true;
}

bool JournalRepository::deleteEntry(uint32_t id) {
    if (!initialized_) return false;
    auto it = entryIdToIdx_.find(id);
    if (it == entryIdToIdx_.end() || entriesIndex_[it->second].isDeleted) {
        lastError_ = "ENTRY_NOT_FOUND";
        return false;
    }

    std::string payload = "{\"id\":" + std::to_string(id) + "}";
    uint32_t offset = 0, len = 0;
    if (!appendRecord(TYPE_ENTRY_DELETE, payload, offset, len)) {
        return false;
    }

    entriesIndex_[it->second].isDeleted = true;
    return true;
}

bool JournalRepository::getEntry(uint32_t id, JournalEntry& outEntry) const {
    if (!initialized_ || !fs_) return false;
    auto it = entryIdToIdx_.find(id);
    if (it == entryIdToIdx_.end() || entriesIndex_[it->second].isDeleted) {
        return false;
    }

    const IndexEntry& ie = entriesIndex_[it->second];
    IFileHandle* file = fs_->open(LOG_PATH, "r");
    if (!file) return false;

    if (!file->seek(ie.fileOffset)) {
        file->close();
        delete file;
        return false;
    }

    std::string payload;
    payload.resize(ie.payloadLength);
    size_t rd = file->read(reinterpret_cast<uint8_t*>(&payload[0]), ie.payloadLength);
    file->close();
    delete file;

    if (rd != ie.payloadLength) return false;
    return deserializeEntry(payload, outEntry);
}

size_t JournalRepository::countEntries() const {
    size_t count = 0;
    for (const auto& ie : entriesIndex_) {
        if (!ie.isDeleted) count++;
    }
    return count;
}

std::vector<uint32_t> JournalRepository::queryEntryIds(const EntryFilter& filter, size_t limit, size_t offset) const {
    std::vector<uint32_t> results;
    size_t skipped = 0;

    // Parcours antichronologique (entrées les plus récentes d'abord)
    for (int i = static_cast<int>(entriesIndex_.size()) - 1; i >= 0; i--) {
        const auto& ie = entriesIndex_[i];
        if (ie.isDeleted) continue;

        if (filter.projectId >= 0 && static_cast<uint32_t>(filter.projectId) != ie.projectId) continue;
        if (filter.type >= 0 && static_cast<EntryType>(filter.type) != ie.type) continue;
        if (filter.status >= 0 && static_cast<EntryStatus>(filter.status) != ie.status) continue;
        if (filter.createdAfter > 0 && ie.createdAt < filter.createdAfter) continue;
        if (filter.createdBefore > 0 && ie.createdAt > filter.createdBefore) continue;
        if (filter.dueAfter > 0 && ie.dueAt < filter.dueAfter) continue;
        if (filter.dueBefore > 0 && ie.dueAt > filter.dueBefore) continue;

        if (skipped < offset) {
            skipped++;
            continue;
        }

        results.push_back(ie.id);
        if (results.size() >= limit) break;
    }

    return results;
}

bool JournalRepository::addProject(Project& project) {
    if (!initialized_) return false;
    project.id = nextProjectId_++;
    std::string payload = serializeProject(project);
    uint32_t offset = 0, len = 0;
    if (!appendRecord(TYPE_PROJECT_DATA, payload, offset, len)) {
        return false;
    }

    IndexProject ip;
    ip.id = project.id;
    ip.fileOffset = offset;
    ip.payloadLength = len;
    ip.status = project.status;
    ip.name = project.name;

    projectIdToIdx_[project.id] = projectsIndex_.size();
    projectsIndex_.push_back(ip);
    return true;
}

bool JournalRepository::updateProject(const Project& project) {
    if (!initialized_) return false;
    auto it = projectIdToIdx_.find(project.id);
    if (it == projectIdToIdx_.end()) {
        lastError_ = "PROJECT_NOT_FOUND";
        return false;
    }

    std::string payload = serializeProject(project);
    uint32_t offset = 0, len = 0;
    if (!appendRecord(TYPE_PROJECT_DATA, payload, offset, len)) {
        return false;
    }

    IndexProject& ip = projectsIndex_[it->second];
    ip.fileOffset = offset;
    ip.payloadLength = len;
    ip.status = project.status;
    ip.name = project.name;
    return true;
}

bool JournalRepository::getProject(uint32_t id, Project& outProject) const {
    if (!initialized_ || !fs_) return false;
    auto it = projectIdToIdx_.find(id);
    if (it == projectIdToIdx_.end()) return false;

    const IndexProject& ip = projectsIndex_[it->second];
    IFileHandle* file = fs_->open(LOG_PATH, "r");
    if (!file) return false;

    file->seek(ip.fileOffset);
    std::string payload;
    payload.resize(ip.payloadLength);
    size_t rd = file->read(reinterpret_cast<uint8_t*>(&payload[0]), ip.payloadLength);
    file->close();
    delete file;

    if (rd != ip.payloadLength) return false;
    return deserializeProject(payload, outProject);
}

std::vector<Project> JournalRepository::listProjects(bool activeOnly) const {
    std::vector<Project> list;
    for (const auto& ip : projectsIndex_) {
        if (activeOnly && ip.status != ProjectStatus::ACTIVE) continue;
        Project p;
        if (getProject(ip.id, p)) {
            list.push_back(p);
        }
    }
    return list;
}

uint32_t JournalRepository::findProjectIdByName(const std::string& name) const {
    for (const auto& ip : projectsIndex_) {
        if (ip.status == ProjectStatus::ACTIVE && ip.name == name) {
            return ip.id;
        }
    }
    return 0;
}

bool JournalRepository::compact() {
    if (!initialized_ || !fs_) return false;

    // 1. Ouvrir le fichier temporaire
    IFileHandle* tmpFile = fs_->open(TMP_PATH, "w");
    if (!tmpFile) {
        lastError_ = "COMPACT_CREATE_TMP_FAILED";
        return false;
    }

    tmpFile->write(reinterpret_cast<const uint8_t*>(FILE_HEADER), sizeof(FILE_HEADER));

    // 2. Écrire tous les projets
    for (const auto& ip : projectsIndex_) {
        Project proj;
        if (getProject(ip.id, proj)) {
            std::string payload = serializeProject(proj);
            uint8_t hdr[7];
            hdr[0] = RECORD_MAGIC0;
            hdr[1] = RECORD_MAGIC1;
            hdr[2] = TYPE_PROJECT_DATA;
            uint32_t pLen = static_cast<uint32_t>(payload.size());
            std::memcpy(&hdr[3], &pLen, 4);
            tmpFile->write(hdr, sizeof(hdr));
            tmpFile->write(reinterpret_cast<const uint8_t*>(payload.data()), pLen);
            uint32_t crc = computeCrc32(reinterpret_cast<const uint8_t*>(payload.data()), pLen);
            tmpFile->write(reinterpret_cast<const uint8_t*>(&crc), 4);
            uint8_t nl = '\n';
            tmpFile->write(&nl, 1);
        }
    }

    // 3. Écrire toutes les entrées actives (non supprimées)
    for (const auto& ie : entriesIndex_) {
        if (ie.isDeleted) continue;
        JournalEntry entry;
        if (getEntry(ie.id, entry)) {
            std::string payload = serializeEntry(entry);
            uint8_t hdr[7];
            hdr[0] = RECORD_MAGIC0;
            hdr[1] = RECORD_MAGIC1;
            hdr[2] = TYPE_ENTRY_DATA;
            uint32_t pLen = static_cast<uint32_t>(payload.size());
            std::memcpy(&hdr[3], &pLen, 4);
            tmpFile->write(hdr, sizeof(hdr));
            tmpFile->write(reinterpret_cast<const uint8_t*>(payload.data()), pLen);
            uint32_t crc = computeCrc32(reinterpret_cast<const uint8_t*>(payload.data()), pLen);
            tmpFile->write(reinterpret_cast<const uint8_t*>(&crc), 4);
            uint8_t nl = '\n';
            tmpFile->write(&nl, 1);
        }
    }

    tmpFile->flush();
    tmpFile->close();
    delete tmpFile;

    // 4. Remplacer atomiquement l'ancien fichier par le nouveau
    fs_->remove(LOG_PATH);
    if (!fs_->rename(TMP_PATH, LOG_PATH)) {
        lastError_ = "COMPACT_RENAME_FAILED";
        return false;
    }

    // 5. Reconstruire l'index
    return begin();
}

size_t JournalRepository::getUsedBytes() const {
    return fs_ ? fs_->usedBytes() : 0;
}

size_t JournalRepository::getTotalBytes() const {
    return fs_ ? fs_->totalBytes() : 0;
}

} // namespace VoiceJournal
