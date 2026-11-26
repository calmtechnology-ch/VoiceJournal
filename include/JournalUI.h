/**
 * @file JournalUI.h
 * @brief Interface utilisateur LVGL 9 (320x240 paysage natif) — VoiceJournal v0.3.0
 */

#pragma once

#include <Arduino.h>
#include <cstdint>
#include <string>
#include <vector>
#include "AppConfig.h"
#include "VoiceController.h"
#include "MemoryModel.h"

#ifdef ARDUINO
#include <lvgl.h>
#include "FrenchFonts.h"
#endif

namespace VoiceJournal {
    class JournalRepository;
    class ProjectService;
    class TaskService;
    class TimeService;
}

/**
 * @enum UiPage
 * @brief Écrans principaux de l'interface LVGL 9.
 */
enum class UiPage : uint8_t {
    HOME = 0,     ///< Accueil épuré
    JOURNAL = 1,  ///< Liste chronologique du journal (ou filtrée)
    PROJECTS = 2, ///< Liste des projets
    TASKS = 3,    ///< Tâches et to-do list
    DETAIL = 4,   ///< Détail complet d'une observation
    DIAGNOSTICS = 5 ///< Diagnostics / Parsing Lab (accès secondaire)
};

/**
 * @enum TouchAction
 * @brief Actions tactiles produites par l'IHM.
 */
enum class TouchAction : uint8_t {
    NONE = 0,
    TALK_TOGGLE,     ///< Clic sur [PARLER / TERMINER]
    STOP,            ///< Clic prioritaire sur [STOP]
    SWITCH_PROVIDER, ///< Clic sur [OPENAI / GEMINI]
    TOGGLE_LAB,      ///< Clic sur accès diagnostics
    CYCLE_LAB_SUBPAGE,
    NAV_HOME,
    NAV_JOURNAL,
    NAV_PROJECTS,
    NAV_TASKS,
    VIEW_ENTRY_DETAIL,
    RESOLVE_ENTRY,
    READ_ENTRY
};

/**
 * @struct TouchSample
 * @brief Échantillon tactile indépendant du matériel pour tests natifs.
 */
struct TouchSample {
    bool isPressed = false;
    bool wasPressed = false;
    int x = 0;
    int y = 0;
};

/**
 * @struct JournalCardSnapshot
 * @brief État normalisé d'une carte du Journal (utilisé par LVGL et les tests natifs).
 */
struct JournalCardSnapshot {
    uint32_t entryId = 0;
    bool visible = false;
    bool readError = false;
    std::string text;
    std::string meta;
    std::string errorReason;
};

/**
 * @struct JournalDisplaySnapshot
 * @brief Instantané logique de la page Journal (COR-ST06).
 */
struct JournalDisplaySnapshot {
    bool littleFsMounted = false;
    bool repositoryReady = false;
    bool filterActive = false;
    size_t totalEntries = 0;
    size_t queryResultCount = 0;
    size_t visibleCards = 0;
    size_t readErrors = 0;
    std::string headerTitle;
    std::string subtitle;
    std::string statusBanner;
    bool statusIsError = false;
    std::vector<JournalCardSnapshot> cards;
};

/**
 * @class JournalUI
 * @brief Interface graphique LVGL 9 pour M5Stack CoreS3 (320x240, rotation native 0).
 */
class JournalUI {
public:
    static constexpr int SCREEN_W = 320;
    static constexpr int SCREEN_H = 240;

    static constexpr int BTN_PROVIDER_X = 184;
    static constexpr int BTN_PROVIDER_Y = 38;
    static constexpr int BTN_PROVIDER_W = 98;
    static constexpr int BTN_PROVIDER_H = 24;

    static constexpr int BTN_Y      = 184;
    static constexpr int BTN_H      = 48;
    static constexpr int BTN_TALK_X = 8;
    static constexpr int BTN_TALK_W = 196;
    static constexpr int BTN_STOP_X = 212;
    static constexpr int BTN_STOP_W = 100;

    JournalUI();
    ~JournalUI();

    /**
     * @brief Associe les services métier pour alimenter les listes LVGL 9.
     */
    void attachServices(VoiceJournal::JournalRepository* repo,
                        VoiceJournal::ProjectService* projService,
                        VoiceJournal::TaskService* taskService,
                        VoiceJournal::TimeService* timeService);

    /**
     * @brief Initialise l'interface LVGL 9 et configure les écrans.
     */
    void begin(const JournalViewModel& initialView);

    /**
     * @brief Met à jour le ViewModel et rafraîchit l'IHM.
     */
    void updateView(const JournalViewModel& view);

    /**
     * @brief Traite les temporisateurs et le rendu LVGL 9 non bloquant.
     */
    void renderIfNeeded();

    /**
     * @brief Interroge les événements tactiles.
     */
    TouchAction pollTouch(uint32_t nowMs);

    /**
     * @brief Traitement logique des touches compatible avec les tests natifs v0.2.1.
     */
    TouchAction processTouchSample(const TouchSample& sample, uint32_t nowMs);

    /**
     * @brief Navigation vers une page spécifique.
     */
    void setPage(UiPage page);
    UiPage getCurrentPage() const { return currentPage_; }

    /**
     * @brief Ouvre la vue de détail d'une entrée.
     */
    void showEntryDetail(uint32_t entryId);

    /**
     * @brief Configure un filtre actif sur la page Journal.
     */
    void setJournalFilter(int32_t projectId, int16_t type = -1, const String& title = "");
    void clearJournalFilter();
    bool hasActiveJournalFilter() const {
        return filterProjectId_ != -1 || filterType_ != -1 || filterTitle_.length() > 0;
    }

    /**
     * @brief Demande le rafraîchissement immédiat de la page Journal (après ajout ou suppression).
     */
    void refreshJournal();

    /**
     * @brief Construit l'instantané d'affichage du Journal (indépendant de LVGL pour audit & tests).
     */
    JournalDisplaySnapshot buildJournalSnapshot() const;

    uint32_t getSelectedDetailEntryId() const { return selectedDetailEntryId_; }

    static String toDisplayAscii(const String& utf8);
    static std::string truncateUtf8Std(const std::string& utf8, size_t maxBytes);

private:
    VoiceJournal::JournalRepository* repo_ = nullptr;
    VoiceJournal::ProjectService* projService_ = nullptr;
    VoiceJournal::TaskService* taskService_ = nullptr;
    VoiceJournal::TimeService* timeService_ = nullptr;

    JournalViewModel view_;
    UiPage currentPage_ = UiPage::HOME;
    uint32_t selectedDetailEntryId_ = 0;
    int32_t filterProjectId_ = -1;
    int16_t filterType_ = -1;
    String filterTitle_;

    bool needsRedraw_ = true;
    bool touchDown_ = false;
    bool hasLastTalkAccepted_ = false;
    uint32_t lastTalkAcceptedMs_ = 0;
    bool hasLastProviderAccepted_ = false;
    uint32_t lastProviderAcceptedMs_ = 0;

    TouchAction pendingAction_ = TouchAction::NONE;

#ifdef ARDUINO
    // Objets LVGL 9
    lv_display_t* disp_ = nullptr;
    lv_indev_t* indev_ = nullptr;

    // Écrans / Conteneurs de pages
    lv_obj_t* screenHome_ = nullptr;
    lv_obj_t* screenJournal_ = nullptr;
    lv_obj_t* screenProjects_ = nullptr;
    lv_obj_t* screenTasks_ = nullptr;
    lv_obj_t* screenDetail_ = nullptr;
    lv_obj_t* screenDiag_ = nullptr;

    // Barre de navigation
    lv_obj_t* navBar_ = nullptr;
    lv_obj_t* btnNavHome_ = nullptr;
    lv_obj_t* btnNavJournal_ = nullptr;
    lv_obj_t* btnNavProjects_ = nullptr;
    lv_obj_t* btnNavTasks_ = nullptr;

    // Bandeau vocal global
    lv_obj_t* voiceBanner_ = nullptr;
    lv_obj_t* voiceBannerStateLabel_ = nullptr;
    lv_obj_t* voiceBannerBtnFinish_ = nullptr;
    lv_obj_t* voiceBannerBtnStop_ = nullptr;

    // Widgets Accueil
    lv_obj_t* lblHomeHeaderTitle_ = nullptr;
    lv_obj_t* lblHomeHeaderState_ = nullptr;
    lv_obj_t* btnHomeProvider_ = nullptr;
    lv_obj_t* lblHomeProvider_ = nullptr;
    lv_obj_t* lblHomeTime_ = nullptr;
    lv_obj_t* lblHomeProject_ = nullptr;
    lv_obj_t* lblHomeBigCounter_ = nullptr;
    lv_obj_t* lblHomeCounterDesc_ = nullptr;
    lv_obj_t* lblHomeStatus_ = nullptr;
    lv_obj_t* btnHomeTalk_ = nullptr;
    lv_obj_t* lblHomeTalk_ = nullptr;
    lv_obj_t* btnHomeStop_ = nullptr;

    // Widgets Journal
    lv_obj_t* lblJournalTitle_ = nullptr;
    lv_obj_t* lblJournalSubtitle_ = nullptr;
    lv_obj_t* btnJournalClearFilter_ = nullptr;
    lv_obj_t* lblJournalEmpty_ = nullptr;
    lv_obj_t* listJournalContainer_ = nullptr;
    std::vector<lv_obj_t*> journalCardObjs_;
    std::vector<uint32_t> journalCardIds_;

    // Widgets Projets
    lv_obj_t* lblProjectsTitle_ = nullptr;
    lv_obj_t* listProjectsContainer_ = nullptr;
    std::vector<lv_obj_t*> projectCardObjs_;
    std::vector<uint32_t> projectCardIds_;

    // Widgets Tâches
    lv_obj_t* lblTasksTitle_ = nullptr;
    lv_obj_t* lblTasksCount_ = nullptr;
    lv_obj_t* listTasksContainer_ = nullptr;
    std::vector<lv_obj_t*> taskCardObjs_;
    std::vector<uint32_t> taskCardIds_;

    // Widgets Détail
    lv_obj_t* btnDetailBack_ = nullptr;
    lv_obj_t* lblDetailTitle_ = nullptr;
    lv_obj_t* lblDetailText_ = nullptr;
    lv_obj_t* lblDetailMetaProject_ = nullptr;
    lv_obj_t* lblDetailMetaDate_ = nullptr;
    lv_obj_t* lblDetailMetaStatus_ = nullptr;
    lv_obj_t* btnDetailRead_ = nullptr;
    lv_obj_t* btnDetailAction_ = nullptr;
    lv_obj_t* lblDetailAction_ = nullptr;

    void buildLvglUi();
    void updateHomeWidgets();
    void updateJournalWidgets();
    void updateProjectsWidgets();
    void updateTasksWidgets();
    void updateDetailWidgets();
    void updateVoiceBanner();
    void updateNavBar();

    static void lvglFlushCallback(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map);
    static void lvglTouchReadCallback(lv_indev_t* indev, lv_indev_data_t* data);
#endif

    bool isTalkButtonEnabled() const;
    bool isProviderSwitchEnabled() const;
    static bool isInsideRect(int x, int y, int rx, int ry, int rw, int rh);
};
