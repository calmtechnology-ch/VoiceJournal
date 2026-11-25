/**
 * @file ProjectService.cpp
 * @brief Implémentation du service de gestion des projets — VoiceJournal v0.3.0
 */

#include "ProjectService.h"
#include "parser/TextNormalizer.h"
#include <algorithm>

namespace VoiceJournal {

std::string ProjectService::normalizeName(const std::string& s) {
    return Parser::TextNormalizer::normalizeSimple(s);
}

ProjectService::ProjectService(JournalRepository& repo, TimeService& timeService)
    : repo_(repo), timeService_(timeService) {
}

bool ProjectService::createProject(const std::string& name, const std::vector<std::string>& aliases, uint32_t& outId) {
    if (name.empty()) return false;

    // Vérifier si un projet de même nom normalisé existe déjà
    std::string normNew = normalizeName(name);
    for (const auto& p : repo_.listProjects(false)) {
        if (normalizeName(p.name) == normNew) {
            outId = p.id;
            return false; // Existe déjà
        }
    }

    Project proj;
    proj.name = name;
    proj.aliases = aliases;
    proj.createdAt = timeService_.getCurrentUtc();
    proj.status = ProjectStatus::ACTIVE;

    if (!repo_.addProject(proj)) {
        return false;
    }

    outId = proj.id;
    return true;
}

bool ProjectService::openProject(const std::string& nameOrAlias, uint32_t& outId) {
    uint32_t pid = findProject(nameOrAlias);
    if (pid == 0) return false;

    currentProjectId_ = pid;
    outId = pid;
    return true;
}

void ProjectService::closeCurrentProject() {
    currentProjectId_ = 0;
}

std::string ProjectService::getCurrentProjectName() const {
    if (currentProjectId_ == 0) return "";
    Project p;
    if (repo_.getProject(currentProjectId_, p)) {
        return p.name;
    }
    return "";
}

uint32_t ProjectService::findProject(const std::string& query) const {
    if (query.empty()) return 0;
    std::string normQuery = normalizeName(query);

    // 1. Recherche exacte par nom ou alias
    for (const auto& p : repo_.listProjects(true)) {
        if (normalizeName(p.name) == normQuery) {
            return p.id;
        }
        for (const auto& alias : p.aliases) {
            if (normalizeName(alias) == normQuery) {
                return p.id;
            }
        }
    }

    // 2. Recherche par sous-chaîne
    for (const auto& p : repo_.listProjects(true)) {
        std::string normP = normalizeName(p.name);
        if (normP.find(normQuery) != std::string::npos || normQuery.find(normP) != std::string::npos) {
            return p.id;
        }
    }

    return 0;
}

std::vector<Project> ProjectService::listProjects(bool activeOnly) const {
    return repo_.listProjects(activeOnly);
}

bool ProjectService::getProject(uint32_t id, Project& outProject) const {
    return repo_.getProject(id, outProject);
}

bool ProjectService::archiveProject(uint32_t id) {
    Project p;
    if (!repo_.getProject(id, p)) return false;
    p.status = ProjectStatus::ARCHIVED;
    if (currentProjectId_ == id) {
        currentProjectId_ = 0;
    }
    return repo_.updateProject(p);
}

} // namespace VoiceJournal
