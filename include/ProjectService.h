/**
 * @file ProjectService.h
 * @brief Gestionnaire des projets et du contexte de projet actif — VoiceJournal v0.3.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "JournalRepository.h"
#include "TimeService.h"

namespace VoiceJournal {

class ProjectService {
public:
    ProjectService(JournalRepository& repo, TimeService& timeService);

    /**
     * @brief Crée un nouveau projet.
     */
    bool createProject(const std::string& name, const std::vector<std::string>& aliases, uint32_t& outId);

    /**
     * @brief Ouvre un projet par son nom ou un de ses alias et le définit comme projet courant.
     */
    bool openProject(const std::string& nameOrAlias, uint32_t& outId);

    /**
     * @brief Ferme le projet courant.
     */
    void closeCurrentProject();

    /**
     * @brief Retourne l'ID du projet courant (0 si aucun).
     */
    uint32_t getCurrentProjectId() const { return currentProjectId_; }

    /**
     * @brief Retourne le nom du projet courant (ou chaîne vide si aucun).
     */
    std::string getCurrentProjectName() const;

    /**
     * @brief Recherche un projet par correspondance approximative ou exacte.
     */
    uint32_t findProject(const std::string& query) const;

    /**
     * @brief Liste tous les projets actifs.
     */
    std::vector<Project> listProjects(bool activeOnly = true) const;

    /**
     * @brief Obtient un projet par son ID.
     */
    bool getProject(uint32_t id, Project& outProject) const;

    /**
     * @brief Archive un projet.
     */
    bool archiveProject(uint32_t id);

private:
    JournalRepository& repo_;
    TimeService& timeService_;
    uint32_t currentProjectId_ = 0;

    static std::string normalizeName(const std::string& s);
};

} // namespace VoiceJournal
