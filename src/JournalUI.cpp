/**
 * @file JournalUI.cpp
 * @brief Implémentation de l'interface LVGL 9 pour M5Stack CoreS3 — VoiceJournal v0.3.0
 */

#include "JournalUI.h"
#include "JournalRepository.h"
#include "ProjectService.h"
#include "TaskService.h"
#include "TimeService.h"

#ifdef ARDUINO
#include <M5Unified.h>

#if VJ_DIAG_ENABLE_LVGL
// Buffer partiel DMA dans la RAM interne (20 lignes = 320 * 20 pixels RGB565 = 12 800 octets)
alignas(4) static uint8_t s_lvglDrawBuf[JournalUI::SCREEN_W * 20 * sizeof(uint16_t)];
#endif
static JournalUI* s_activeUi = nullptr;
#endif

// Palette graphique normalisée CalmTechnology (Section 18)
#define COLOR_HEX_BG         0x101820
#define COLOR_HEX_CARD       0x1D2A36
#define COLOR_HEX_SEP        0x2B3946
#define COLOR_HEX_TXT_PRI    0xF4F7FA
#define COLOR_HEX_TXT_SEC    0xA9B7C6
#define COLOR_HEX_ACTION     0x236AC2
#define COLOR_HEX_STOP       0xA93440
#define COLOR_HEX_SUCCESS    0x65C9A0
#define COLOR_HEX_PROBLEM    0xE77878
#define COLOR_HEX_DUE        0xE0BA75
#define COLOR_HEX_SELECTION  0x65B2FF

JournalUI::JournalUI() {
#ifdef ARDUINO
    s_activeUi = this;
#endif
}

JournalUI::~JournalUI() {
#ifdef ARDUINO
    if (s_activeUi == this) {
        s_activeUi = nullptr;
    }
#endif
}

void JournalUI::attachServices(VoiceJournal::JournalRepository* repo,
                              VoiceJournal::ProjectService* projService,
                              VoiceJournal::TaskService* taskService,
                              VoiceJournal::TimeService* timeService) {
    repo_ = repo;
    projService_ = projService;
    taskService_ = taskService;
    timeService_ = timeService;
}

#ifdef ARDUINO

void JournalUI::lvglFlushCallback(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    M5.Display.startWrite();
    M5.Display.pushImage(area->x1, area->y1, w, h, (const uint16_t*)px_map);
    M5.Display.endWrite();

    lv_display_flush_ready(disp);
}

void JournalUI::lvglTouchReadCallback(lv_indev_t* indev, lv_indev_data_t* data) {
    (void)indev;
    if (M5.Touch.getCount() > 0) {
        auto detail = M5.Touch.getDetail(0);
        if (detail.isPressed() || detail.wasPressed()) {
            data->state = LV_INDEV_STATE_PRESSED;
            data->point.x = detail.x;
            data->point.y = detail.y;
            return;
        }
    }
    data->state = LV_INDEV_STATE_RELEASED;
}

static void onNavBtnClicked(lv_event_t* e) {
    auto targetPage = static_cast<UiPage>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    if (s_activeUi) {
        s_activeUi->setPage(targetPage);
    }
}

void JournalUI::buildLvglUi() {
#if !VJ_DIAG_ENABLE_LVGL
    // Mode diagnostic sans LVGL : écran minimal M5.Display
    M5.Display.waitDisplay();
    M5.Display.setRotation(0);
    M5.Display.setBrightness(220);
    M5.Display.fillScreen(COLOR_HEX_BG);
    M5.Display.setTextColor(COLOR_HEX_TXT_PRI, COLOR_HEX_BG);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 12);
    M5.Display.println("VoiceJournal v0.3.0");
    M5.Display.setTextSize(1);
    M5.Display.setCursor(10, 40);
    M5.Display.println("Mode diagnostic (LVGL OFF)");
    return;
#else
    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return static_cast<uint32_t>(millis()); });

    // Orientation paysage native 320x240 à rotation 0 (règle m5stack-cores3-display)
    M5.Display.waitDisplay();
    M5.Display.setRotation(0);
    M5.Display.setBrightness(220);
    M5.Display.fillScreen(COLOR_HEX_BG);

    disp_ = lv_display_create(SCREEN_W, SCREEN_H);
    if (!disp_) return;

    lv_display_set_buffers(disp_, s_lvglDrawBuf, nullptr, sizeof(s_lvglDrawBuf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(disp_, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_flush_cb(disp_, lvglFlushCallback);

    indev_ = lv_indev_create();
    if (indev_) {
        lv_indev_set_type(indev_, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev_, lvglTouchReadCallback);
        lv_timer_t* readTimer = lv_indev_get_read_timer(indev_);
        if (readTimer) {
            lv_timer_set_period(readTimer, 15);
        }
    }

    // Appliquer le thème sombre général et désactiver le scroll parasite sur l'écran racine
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(COLOR_HEX_BG), 0);
    lv_obj_remove_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLLABLE);

    // 1. Barre de navigation inférieure (4 onglets ergonomiques de 30 px de haut à y=204)
    navBar_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(navBar_, 320, 36);
    lv_obj_set_pos(navBar_, 0, 204);
    lv_obj_set_style_bg_color(navBar_, lv_color_hex(COLOR_HEX_CARD), 0);
    lv_obj_set_style_border_color(navBar_, lv_color_hex(COLOR_HEX_SEP), 0);
    lv_obj_set_style_border_width(navBar_, 1, 0);
    lv_obj_set_style_pad_all(navBar_, 1, 0);
    lv_obj_set_scrollbar_mode(navBar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(navBar_, LV_OBJ_FLAG_SCROLLABLE);

    auto makeNavBtn = [this](lv_obj_t*& btn, const char* label, UiPage p, int x) {
        btn = lv_button_create(navBar_);
        lv_obj_set_size(btn, 76, 30);
        lv_obj_set_pos(btn, x, 2);
        lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_HEX_CARD), 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(btn, onNavBtnClicked, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<uintptr_t>(p)));

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_font(lbl, &font_french_12, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COLOR_HEX_TXT_SEC), 0);
        lv_obj_center(lbl);
    };

    makeNavBtn(btnNavHome_, "Accueil", UiPage::HOME, 2);
    makeNavBtn(btnNavJournal_, "Journal", UiPage::JOURNAL, 81);
    makeNavBtn(btnNavProjects_, "Projets", UiPage::PROJECTS, 160);
    makeNavBtn(btnNavTasks_, "Tâches", UiPage::TASKS, 239);

    // 2. Écran 1 : Accueil (Home)
    screenHome_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(screenHome_, 320, 204);
    lv_obj_set_pos(screenHome_, 0, 0);
    lv_obj_set_style_bg_color(screenHome_, lv_color_hex(COLOR_HEX_BG), 0);
    lv_obj_set_style_border_width(screenHome_, 0, 0);
    lv_obj_set_style_pad_all(screenHome_, 6, 0);
    lv_obj_set_scrollbar_mode(screenHome_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(screenHome_, LV_OBJ_FLAG_SCROLLABLE);

    // Titre et état
    lblHomeHeaderTitle_ = lv_label_create(screenHome_);
    lv_label_set_text(lblHomeHeaderTitle_, "VoiceJournal v0.3.0");
    lv_obj_set_style_text_font(lblHomeHeaderTitle_, &font_french_bold_16, 0);
    lv_obj_set_style_text_color(lblHomeHeaderTitle_, lv_color_hex(COLOR_HEX_TXT_PRI), 0);
    lv_obj_set_pos(lblHomeHeaderTitle_, 4, 2);

    lblHomeHeaderState_ = lv_label_create(screenHome_);
    lv_label_set_text(lblHomeHeaderState_, "PRÊT");
    lv_obj_set_style_text_font(lblHomeHeaderState_, &font_french_12, 0);
    lv_obj_set_style_text_color(lblHomeHeaderState_, lv_color_hex(COLOR_HEX_SUCCESS), 0);
    lv_obj_align(lblHomeHeaderState_, LV_ALIGN_TOP_RIGHT, -4, 4);

    // Sous-titre : Bouton fournisseur cliquable et heure
    btnHomeProvider_ = lv_button_create(screenHome_);
    lv_obj_set_size(btnHomeProvider_, 84, 26);
    lv_obj_set_pos(btnHomeProvider_, 4, 24);
    lv_obj_set_style_bg_color(btnHomeProvider_, lv_color_hex(COLOR_HEX_CARD), 0);
    lv_obj_set_style_border_color(btnHomeProvider_, lv_color_hex(COLOR_HEX_SELECTION), 0);
    lv_obj_set_style_border_width(btnHomeProvider_, 1, 0);
    lv_obj_set_style_radius(btnHomeProvider_, 6, 0);
    lv_obj_set_style_shadow_width(btnHomeProvider_, 0, 0);
    lv_obj_set_style_pad_all(btnHomeProvider_, 0, 0);
    lv_obj_remove_flag(btnHomeProvider_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btnHomeProvider_, [](lv_event_t* e) {
        (void)e;
        if (!s_activeUi || !s_activeUi->isProviderSwitchEnabled()) return;
        const uint32_t nowMs = millis();
        if (s_activeUi->hasLastProviderAccepted_ &&
            (nowMs - s_activeUi->lastProviderAcceptedMs_) < AppConfig::PROVIDER_SWITCH_DEBOUNCE_MS) {
            return;
        }
        s_activeUi->hasLastProviderAccepted_ = true;
        s_activeUi->lastProviderAcceptedMs_ = nowMs;
        s_activeUi->pendingAction_ = TouchAction::SWITCH_PROVIDER;
    }, LV_EVENT_CLICKED, nullptr);

    lblHomeProvider_ = lv_label_create(btnHomeProvider_);
    lv_label_set_text(lblHomeProvider_, "OpenAI");
    lv_obj_set_style_text_font(lblHomeProvider_, &font_french_14, 0);
    lv_obj_set_style_text_color(lblHomeProvider_, lv_color_hex(COLOR_HEX_SELECTION), 0);
    lv_obj_center(lblHomeProvider_);

    lblHomeTime_ = lv_label_create(screenHome_);
    lv_label_set_text(lblHomeTime_, "--:--");
    lv_obj_set_style_text_font(lblHomeTime_, &font_french_14, 0);
    lv_obj_set_style_text_color(lblHomeTime_, lv_color_hex(COLOR_HEX_TXT_SEC), 0);
    lv_obj_align(lblHomeTime_, LV_ALIGN_TOP_RIGHT, -4, 28);

    lblHomeProject_ = lv_label_create(screenHome_);
    lv_label_set_text(lblHomeProject_, "");
    lv_obj_set_style_text_font(lblHomeProject_, &font_french_12, 0);
    lv_obj_set_style_text_color(lblHomeProject_, lv_color_hex(COLOR_HEX_DUE), 0);
    lv_obj_set_pos(lblHomeProject_, 94, 30);

    // Compteur central
    lblHomeBigCounter_ = lv_label_create(screenHome_);
    lv_label_set_text(lblHomeBigCounter_, "0");
    lv_obj_set_style_text_font(lblHomeBigCounter_, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lblHomeBigCounter_, lv_color_hex(COLOR_HEX_TXT_PRI), 0);
    lv_obj_set_pos(lblHomeBigCounter_, 140, 62);

    lblHomeCounterDesc_ = lv_label_create(screenHome_);
    lv_label_set_text(lblHomeCounterDesc_, "entrées en mémoire");
    lv_obj_set_style_text_font(lblHomeCounterDesc_, &font_french_14, 0);
    lv_obj_set_style_text_color(lblHomeCounterDesc_, lv_color_hex(COLOR_HEX_TXT_SEC), 0);
    lv_obj_set_pos(lblHomeCounterDesc_, 88, 96);

    lblHomeStatus_ = lv_label_create(screenHome_);
    lv_label_set_text(lblHomeStatus_, "Initialisation...");
    lv_obj_set_style_text_font(lblHomeStatus_, &font_french_12, 0);
    lv_obj_set_style_text_color(lblHomeStatus_, lv_color_hex(COLOR_HEX_TXT_SEC), 0);
    lv_label_set_long_mode(lblHomeStatus_, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(lblHomeStatus_, 300, 18);
    lv_obj_set_pos(lblHomeStatus_, 4, 120);

    // Boutons d'action : PARLER (largeur 190) et STOP (largeur 104)
    btnHomeTalk_ = lv_button_create(screenHome_);
    lv_obj_set_size(btnHomeTalk_, 190, 46);
    lv_obj_set_pos(btnHomeTalk_, 4, 144);
    lv_obj_set_style_bg_color(btnHomeTalk_, lv_color_hex(COLOR_HEX_ACTION), 0);
    lv_obj_set_style_radius(btnHomeTalk_, 8, 0);
    lv_obj_remove_flag(btnHomeTalk_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btnHomeTalk_, [](lv_event_t* e) {
        (void)e;
        if (!s_activeUi || !s_activeUi->isTalkButtonEnabled()) return;
        const uint32_t nowMs = millis();
        if (s_activeUi->hasLastTalkAccepted_ &&
            (nowMs - s_activeUi->lastTalkAcceptedMs_) < AppConfig::TOUCH_DEBOUNCE_MS) {
            return;
        }
        s_activeUi->hasLastTalkAccepted_ = true;
        s_activeUi->lastTalkAcceptedMs_ = nowMs;
        s_activeUi->pendingAction_ = TouchAction::TALK_TOGGLE;
    }, LV_EVENT_CLICKED, nullptr);
    lblHomeTalk_ = lv_label_create(btnHomeTalk_);
    lv_label_set_text(lblHomeTalk_, "PARLER");
    lv_obj_set_style_text_font(lblHomeTalk_, &font_french_bold_16, 0);
    lv_obj_center(lblHomeTalk_);

    btnHomeStop_ = lv_button_create(screenHome_);
    lv_obj_set_size(btnHomeStop_, 104, 46);
    lv_obj_set_pos(btnHomeStop_, 200, 144);
    lv_obj_set_style_bg_color(btnHomeStop_, lv_color_hex(COLOR_HEX_STOP), 0);
    lv_obj_set_style_radius(btnHomeStop_, 8, 0);
    lv_obj_remove_flag(btnHomeStop_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btnHomeStop_, [](lv_event_t* e) {
        (void)e;
        if (s_activeUi) {
            s_activeUi->pendingAction_ = TouchAction::STOP;
        }
    }, LV_EVENT_PRESSED, nullptr);
    lv_obj_t* lblStop = lv_label_create(btnHomeStop_);
    lv_label_set_text(lblStop, "STOP");
    lv_obj_set_style_text_font(lblStop, &font_french_bold_16, 0);
    lv_obj_center(lblStop);

    // 3. Écran 2 : Journal
    screenJournal_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(screenJournal_, 320, 204);
    lv_obj_set_pos(screenJournal_, 0, 0);
    lv_obj_set_style_bg_color(screenJournal_, lv_color_hex(COLOR_HEX_BG), 0);
    lv_obj_set_style_border_width(screenJournal_, 0, 0);
    lv_obj_set_style_pad_all(screenJournal_, 4, 0);
    lv_obj_set_scrollbar_mode(screenJournal_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(screenJournal_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screenJournal_, LV_OBJ_FLAG_HIDDEN);

    lblJournalTitle_ = lv_label_create(screenJournal_);
    lv_label_set_text(lblJournalTitle_, "Journal");
    lv_obj_set_style_text_font(lblJournalTitle_, &font_french_bold_16, 0);
    lv_obj_set_style_text_color(lblJournalTitle_, lv_color_hex(COLOR_HEX_TXT_PRI), 0);
    lv_obj_set_pos(lblJournalTitle_, 6, 2);

    btnJournalClearFilter_ = lv_button_create(screenJournal_);
    lv_obj_set_size(btnJournalClearFilter_, 64, 20);
    lv_obj_set_pos(btnJournalClearFilter_, 154, 1);
    lv_obj_set_style_bg_color(btnJournalClearFilter_, lv_color_hex(COLOR_HEX_CARD), 0);
    lv_obj_set_style_border_color(btnJournalClearFilter_, lv_color_hex(COLOR_HEX_DUE), 0);
    lv_obj_set_style_border_width(btnJournalClearFilter_, 1, 0);
    lv_obj_set_style_radius(btnJournalClearFilter_, 4, 0);
    lv_obj_set_style_pad_all(btnJournalClearFilter_, 0, 0);
    lv_obj_remove_flag(btnJournalClearFilter_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btnJournalClearFilter_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(btnJournalClearFilter_, [](lv_event_t* e) {
        (void)e;
        if (s_activeUi) {
            s_activeUi->clearJournalFilter();
        }
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lblClearFilter = lv_label_create(btnJournalClearFilter_);
    lv_label_set_text(lblClearFilter, "× Filtre");
    lv_obj_set_style_text_font(lblClearFilter, &font_french_12, 0);
    lv_obj_set_style_text_color(lblClearFilter, lv_color_hex(COLOR_HEX_DUE), 0);
    lv_obj_center(lblClearFilter);

    lblJournalSubtitle_ = lv_label_create(screenJournal_);
    lv_label_set_text(lblJournalSubtitle_, "0 entrées");
    lv_obj_set_style_text_font(lblJournalSubtitle_, &font_french_12, 0);
    lv_obj_set_style_text_color(lblJournalSubtitle_, lv_color_hex(COLOR_HEX_TXT_SEC), 0);
    lv_obj_align(lblJournalSubtitle_, LV_ALIGN_TOP_RIGHT, -6, 4);

    listJournalContainer_ = lv_obj_create(screenJournal_);
    lv_obj_set_size(listJournalContainer_, 312, 170);
    lv_obj_set_pos(listJournalContainer_, 0, 24);
    lv_obj_set_style_bg_color(listJournalContainer_, lv_color_hex(COLOR_HEX_BG), 0);
    lv_obj_set_style_border_width(listJournalContainer_, 0, 0);
    lv_obj_set_style_pad_all(listJournalContainer_, 2, 0);
    lv_obj_set_scrollbar_mode(listJournalContainer_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(listJournalContainer_, LV_OBJ_FLAG_SCROLLABLE);

    lblJournalEmpty_ = lv_label_create(listJournalContainer_);
    lv_label_set_text(lblJournalEmpty_, "");
    lv_obj_set_style_text_font(lblJournalEmpty_, &font_french_14, 0);
    lv_obj_set_style_text_color(lblJournalEmpty_, lv_color_hex(COLOR_HEX_TXT_SEC), 0);
    lv_label_set_long_mode(lblJournalEmpty_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lblJournalEmpty_, 296);
    lv_obj_set_pos(lblJournalEmpty_, 6, 48);
    lv_obj_add_flag(lblJournalEmpty_, LV_OBJ_FLAG_HIDDEN);

    // 3 cartes d'entrées réutilisées pour afficher 3 observations visibles simultanément (Section 20)
    journalCardIds_.assign(3, 0);
    for (int i = 0; i < 3; i++) {
        lv_obj_t* card = lv_obj_create(listJournalContainer_);
        lv_obj_set_size(card, 308, 50);
        lv_obj_set_pos(card, 0, i * 54);
        lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_HEX_CARD), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(COLOR_HEX_SEP), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 6, 0);
        lv_obj_set_style_pad_all(card, 4, 0);
        lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, [](lv_event_t* e) {
            const size_t idx = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
            if (s_activeUi && idx < s_activeUi->journalCardIds_.size() && s_activeUi->journalCardIds_[idx] > 0) {
                s_activeUi->showEntryDetail(s_activeUi->journalCardIds_[idx]);
            }
        }, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<uintptr_t>(i)));

        lv_obj_t* lblText = lv_label_create(card);
        lv_label_set_text(lblText, "");
        lv_obj_set_style_text_font(lblText, &font_french_14, 0);
        lv_obj_set_style_text_color(lblText, lv_color_hex(COLOR_HEX_TXT_PRI), 0);
        lv_obj_set_pos(lblText, 4, 2);

        lv_obj_t* lblMeta = lv_label_create(card);
        lv_label_set_text(lblMeta, "");
        lv_obj_set_style_text_font(lblMeta, &font_french_12, 0);
        lv_obj_set_style_text_color(lblMeta, lv_color_hex(COLOR_HEX_TXT_SEC), 0);
        lv_obj_set_pos(lblMeta, 4, 22);

        lv_obj_t* lblChev = lv_label_create(card);
        lv_label_set_text(lblChev, ">");
        lv_obj_set_style_text_font(lblChev, &font_french_14, 0);
        lv_obj_set_style_text_color(lblChev, lv_color_hex(COLOR_HEX_TXT_SEC), 0);
        lv_obj_align(lblChev, LV_ALIGN_RIGHT_MID, -4, 0);

        journalCardObjs_.push_back(card);
    }

    // 4. Écran 3 : Projets
    screenProjects_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(screenProjects_, 320, 204);
    lv_obj_set_pos(screenProjects_, 0, 0);
    lv_obj_set_style_bg_color(screenProjects_, lv_color_hex(COLOR_HEX_BG), 0);
    lv_obj_set_style_border_width(screenProjects_, 0, 0);
    lv_obj_set_style_pad_all(screenProjects_, 4, 0);
    lv_obj_set_scrollbar_mode(screenProjects_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(screenProjects_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screenProjects_, LV_OBJ_FLAG_HIDDEN);

    lblProjectsTitle_ = lv_label_create(screenProjects_);
    lv_label_set_text(lblProjectsTitle_, "Projets");
    lv_obj_set_style_text_font(lblProjectsTitle_, &font_french_bold_16, 0);
    lv_obj_set_style_text_color(lblProjectsTitle_, lv_color_hex(COLOR_HEX_TXT_PRI), 0);
    lv_obj_set_pos(lblProjectsTitle_, 6, 2);

    listProjectsContainer_ = lv_obj_create(screenProjects_);
    lv_obj_set_size(listProjectsContainer_, 312, 170);
    lv_obj_set_pos(listProjectsContainer_, 0, 24);
    lv_obj_set_style_bg_color(listProjectsContainer_, lv_color_hex(COLOR_HEX_BG), 0);
    lv_obj_set_style_border_width(listProjectsContainer_, 0, 0);
    lv_obj_set_style_pad_all(listProjectsContainer_, 2, 0);
    lv_obj_set_scrollbar_mode(listProjectsContainer_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(listProjectsContainer_, LV_OBJ_FLAG_SCROLLABLE);

    projectCardIds_.assign(3, 0);
    for (int i = 0; i < 3; i++) {
        lv_obj_t* card = lv_obj_create(listProjectsContainer_);
        lv_obj_set_size(card, 308, 50);
        lv_obj_set_pos(card, 0, i * 54);
        lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_HEX_CARD), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(COLOR_HEX_SEP), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 6, 0);
        lv_obj_set_style_pad_all(card, 4, 0);
        lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, [](lv_event_t* e) {
            const size_t idx = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
            if (s_activeUi && idx < s_activeUi->projectCardIds_.size() && s_activeUi->projectCardIds_[idx] > 0) {
                s_activeUi->setJournalFilter(static_cast<int32_t>(s_activeUi->projectCardIds_[idx]));
            }
        }, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<uintptr_t>(i)));

        lv_obj_t* lblName = lv_label_create(card);
        lv_label_set_text(lblName, "");
        lv_obj_set_style_text_font(lblName, &font_french_bold_16, 0);
        lv_obj_set_style_text_color(lblName, lv_color_hex(COLOR_HEX_TXT_PRI), 0);
        lv_obj_set_pos(lblName, 4, 2);

        lv_obj_t* lblMeta = lv_label_create(card);
        lv_label_set_text(lblMeta, "");
        lv_obj_set_style_text_font(lblMeta, &font_french_12, 0);
        lv_obj_set_style_text_color(lblMeta, lv_color_hex(COLOR_HEX_TXT_SEC), 0);
        lv_obj_set_pos(lblMeta, 4, 22);

        projectCardObjs_.push_back(card);
    }

    // 5. Écran 4 : Tâches
    screenTasks_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(screenTasks_, 320, 204);
    lv_obj_set_pos(screenTasks_, 0, 0);
    lv_obj_set_style_bg_color(screenTasks_, lv_color_hex(COLOR_HEX_BG), 0);
    lv_obj_set_style_border_width(screenTasks_, 0, 0);
    lv_obj_set_style_pad_all(screenTasks_, 4, 0);
    lv_obj_set_scrollbar_mode(screenTasks_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(screenTasks_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screenTasks_, LV_OBJ_FLAG_HIDDEN);

    lblTasksTitle_ = lv_label_create(screenTasks_);
    lv_label_set_text(lblTasksTitle_, "Tâches");
    lv_obj_set_style_text_font(lblTasksTitle_, &font_french_bold_16, 0);
    lv_obj_set_style_text_color(lblTasksTitle_, lv_color_hex(COLOR_HEX_TXT_PRI), 0);
    lv_obj_set_pos(lblTasksTitle_, 6, 2);

    lblTasksCount_ = lv_label_create(screenTasks_);
    lv_label_set_text(lblTasksCount_, "0 à faire");
    lv_obj_set_style_text_font(lblTasksCount_, &font_french_12, 0);
    lv_obj_set_style_text_color(lblTasksCount_, lv_color_hex(COLOR_HEX_TXT_SEC), 0);
    lv_obj_align(lblTasksCount_, LV_ALIGN_TOP_RIGHT, -6, 4);

    listTasksContainer_ = lv_obj_create(screenTasks_);
    lv_obj_set_size(listTasksContainer_, 312, 170);
    lv_obj_set_pos(listTasksContainer_, 0, 24);
    lv_obj_set_style_bg_color(listTasksContainer_, lv_color_hex(COLOR_HEX_BG), 0);
    lv_obj_set_style_border_width(listTasksContainer_, 0, 0);
    lv_obj_set_style_pad_all(listTasksContainer_, 2, 0);
    lv_obj_set_scrollbar_mode(listTasksContainer_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(listTasksContainer_, LV_OBJ_FLAG_SCROLLABLE);

    taskCardIds_.assign(3, 0);
    for (int i = 0; i < 3; i++) {
        lv_obj_t* card = lv_obj_create(listTasksContainer_);
        lv_obj_set_size(card, 308, 50);
        lv_obj_set_pos(card, 0, i * 54);
        lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_HEX_CARD), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(COLOR_HEX_SEP), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 6, 0);
        lv_obj_set_style_pad_all(card, 4, 0);
        lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* cb = lv_checkbox_create(card);
        lv_checkbox_set_text(cb, "");
        lv_obj_set_pos(cb, 2, 8);
        lv_obj_add_event_cb(cb, [](lv_event_t* e) {
            const size_t idx = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
            if (!s_activeUi || !s_activeUi->repo_ || idx >= s_activeUi->taskCardIds_.size()) return;
            const uint32_t tid = s_activeUi->taskCardIds_[idx];
            if (tid == 0) return;
            VoiceJournal::JournalEntry entry;
            if (s_activeUi->repo_->getEntry(tid, entry)) {
                lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
                const bool checked = lv_obj_has_state(target, LV_STATE_CHECKED);
                entry.status = checked ? VoiceJournal::EntryStatus::COMPLETED
                                       : VoiceJournal::EntryStatus::OPEN;
                if (s_activeUi->timeService_) {
                    entry.updatedAt = s_activeUi->timeService_->getCurrentUtc();
                }
                s_activeUi->repo_->updateEntry(entry);
                s_activeUi->needsRedraw_ = true;
            }
        }, LV_EVENT_VALUE_CHANGED, reinterpret_cast<void*>(static_cast<uintptr_t>(i)));

        lv_obj_t* lblDesc = lv_label_create(card);
        lv_label_set_text(lblDesc, "");
        lv_obj_set_style_text_font(lblDesc, &font_french_14, 0);
        lv_obj_set_style_text_color(lblDesc, lv_color_hex(COLOR_HEX_TXT_PRI), 0);
        lv_obj_set_pos(lblDesc, 34, 2);

        lv_obj_t* lblMeta = lv_label_create(card);
        lv_label_set_text(lblMeta, "");
        lv_obj_set_style_text_font(lblMeta, &font_french_12, 0);
        lv_obj_set_style_text_color(lblMeta, lv_color_hex(COLOR_HEX_TXT_SEC), 0);
        lv_obj_set_pos(lblMeta, 34, 22);

        taskCardObjs_.push_back(card);
    }

    // 6. Écran Détail d'une entrée
    screenDetail_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(screenDetail_, 320, 240);
    lv_obj_set_pos(screenDetail_, 0, 0);
    lv_obj_set_style_bg_color(screenDetail_, lv_color_hex(COLOR_HEX_BG), 0);
    lv_obj_set_style_border_width(screenDetail_, 0, 0);
    lv_obj_set_style_pad_all(screenDetail_, 6, 0);
    lv_obj_set_scrollbar_mode(screenDetail_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(screenDetail_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screenDetail_, LV_OBJ_FLAG_HIDDEN);

    btnDetailBack_ = lv_button_create(screenDetail_);
    lv_obj_set_size(btnDetailBack_, 92, 30);
    lv_obj_set_pos(btnDetailBack_, 4, 2);
    lv_obj_set_style_bg_color(btnDetailBack_, lv_color_hex(COLOR_HEX_CARD), 0);
    lv_obj_remove_flag(btnDetailBack_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* lblBack = lv_label_create(btnDetailBack_);
    lv_label_set_text(lblBack, "← Retour");
    lv_obj_set_style_text_font(lblBack, &font_french_12, 0);
    lv_obj_center(lblBack);
    lv_obj_add_event_cb(btnDetailBack_, [](lv_event_t* e) {
        (void)e;
        if (s_activeUi) s_activeUi->setPage(UiPage::JOURNAL);
    }, LV_EVENT_CLICKED, nullptr);

    lblDetailTitle_ = lv_label_create(screenDetail_);
    lv_label_set_text(lblDetailTitle_, "Observation");
    lv_obj_set_style_text_font(lblDetailTitle_, &font_french_bold_16, 0);
    lv_obj_set_style_text_color(lblDetailTitle_, lv_color_hex(COLOR_HEX_SELECTION), 0);
    lv_obj_align(lblDetailTitle_, LV_ALIGN_TOP_RIGHT, -6, 6);

    lblDetailText_ = lv_label_create(screenDetail_);
    lv_label_set_text(lblDetailText_, "");
    lv_obj_set_style_text_font(lblDetailText_, &font_french_14, 0);
    lv_obj_set_style_text_color(lblDetailText_, lv_color_hex(COLOR_HEX_TXT_PRI), 0);
    lv_label_set_long_mode(lblDetailText_, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(lblDetailText_, 308, 90);
    lv_obj_set_pos(lblDetailText_, 6, 36);

    lblDetailMetaProject_ = lv_label_create(screenDetail_);
    lv_label_set_text(lblDetailMetaProject_, "Projet : -");
    lv_obj_set_style_text_font(lblDetailMetaProject_, &font_french_12, 0);
    lv_obj_set_style_text_color(lblDetailMetaProject_, lv_color_hex(COLOR_HEX_TXT_SEC), 0);
    lv_obj_set_pos(lblDetailMetaProject_, 6, 134);

    lblDetailMetaDate_ = lv_label_create(screenDetail_);
    lv_label_set_text(lblDetailMetaDate_, "Date : -");
    lv_obj_set_style_text_font(lblDetailMetaDate_, &font_french_12, 0);
    lv_obj_set_style_text_color(lblDetailMetaDate_, lv_color_hex(COLOR_HEX_TXT_SEC), 0);
    lv_obj_set_pos(lblDetailMetaDate_, 6, 152);

    lblDetailMetaStatus_ = lv_label_create(screenDetail_);
    lv_label_set_text(lblDetailMetaStatus_, "Statut : -");
    lv_obj_set_style_text_font(lblDetailMetaStatus_, &font_french_12, 0);
    lv_obj_set_style_text_color(lblDetailMetaStatus_, lv_color_hex(COLOR_HEX_TXT_SEC), 0);
    lv_obj_set_pos(lblDetailMetaStatus_, 6, 170);

    // Boutons d'action dans le détail
    btnDetailRead_ = lv_button_create(screenDetail_);
    lv_obj_set_size(btnDetailRead_, 96, 36);
    lv_obj_set_pos(btnDetailRead_, 6, 192);
    lv_obj_set_style_bg_color(btnDetailRead_, lv_color_hex(COLOR_HEX_ACTION), 0);
    lv_obj_remove_flag(btnDetailRead_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* lblRead = lv_label_create(btnDetailRead_);
    lv_label_set_text(lblRead, "LIRE");
    lv_obj_set_style_text_font(lblRead, &font_french_bold_16, 0);
    lv_obj_center(lblRead);
    lv_obj_add_event_cb(btnDetailRead_, [](lv_event_t* e) {
        (void)e;
        if (s_activeUi) s_activeUi->pendingAction_ = TouchAction::READ_ENTRY;
    }, LV_EVENT_CLICKED, nullptr);

    btnDetailAction_ = lv_button_create(screenDetail_);
    lv_obj_set_size(btnDetailAction_, 108, 36);
    lv_obj_set_pos(btnDetailAction_, 110, 192);
    lv_obj_set_style_bg_color(btnDetailAction_, lv_color_hex(COLOR_HEX_SUCCESS), 0);
    lv_obj_remove_flag(btnDetailAction_, LV_OBJ_FLAG_SCROLLABLE);
    lblDetailAction_ = lv_label_create(btnDetailAction_);
    lv_label_set_text(lblDetailAction_, "RÉSOLU");
    lv_obj_set_style_text_font(lblDetailAction_, &font_french_bold_16, 0);
    lv_obj_center(lblDetailAction_);
    lv_obj_add_event_cb(btnDetailAction_, [](lv_event_t* e) {
        (void)e;
        if (s_activeUi) s_activeUi->pendingAction_ = TouchAction::RESOLVE_ENTRY;
    }, LV_EVENT_CLICKED, nullptr);

    // 7. Bandeau vocal global (overlay permanent pendant écoute ou réponse, Section 24)
    voiceBanner_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(voiceBanner_, 320, 40);
    lv_obj_set_pos(voiceBanner_, 0, 200);
    lv_obj_set_style_bg_color(voiceBanner_, lv_color_hex(COLOR_HEX_CARD), 0);
    lv_obj_set_style_border_color(voiceBanner_, lv_color_hex(COLOR_HEX_ACTION), 0);
    lv_obj_set_style_border_width(voiceBanner_, 1, 0);
    lv_obj_set_style_pad_all(voiceBanner_, 2, 0);
    lv_obj_set_scrollbar_mode(voiceBanner_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(voiceBanner_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(voiceBanner_, LV_OBJ_FLAG_HIDDEN);

    voiceBannerStateLabel_ = lv_label_create(voiceBanner_);
    lv_label_set_text(voiceBannerStateLabel_, "ÉCOUTE EN COURS...");
    lv_obj_set_style_text_font(voiceBannerStateLabel_, &font_french_bold_16, 0);
    lv_obj_set_style_text_color(voiceBannerStateLabel_, lv_color_hex(COLOR_HEX_TXT_PRI), 0);
    lv_obj_align(voiceBannerStateLabel_, LV_ALIGN_LEFT_MID, 8, 0);

    voiceBannerBtnStop_ = lv_button_create(voiceBanner_);
    lv_obj_set_size(voiceBannerBtnStop_, 78, 32);
    lv_obj_align(voiceBannerBtnStop_, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(voiceBannerBtnStop_, lv_color_hex(COLOR_HEX_STOP), 0);
    lv_obj_remove_flag(voiceBannerBtnStop_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(voiceBannerBtnStop_, [](lv_event_t* e) {
        (void)e;
        if (s_activeUi) {
            s_activeUi->pendingAction_ = TouchAction::STOP;
        }
    }, LV_EVENT_PRESSED, nullptr);
    lv_obj_t* lblVStop = lv_label_create(voiceBannerBtnStop_);
    lv_label_set_text(lblVStop, "STOP");
    lv_obj_set_style_text_font(lblVStop, &font_french_bold_16, 0);
    lv_obj_center(lblVStop);
#endif
}

void JournalUI::updateHomeWidgets() {
    if (!lblHomeHeaderState_) return;

    lv_label_set_text(lblHomeHeaderState_, journalStateToUiLabel(view_.state));
    lv_obj_set_style_text_color(lblHomeHeaderState_,
        (view_.state == JournalState::READY) ? lv_color_hex(COLOR_HEX_SUCCESS) :
        (view_.state == JournalState::ERROR_STATE) ? lv_color_hex(COLOR_HEX_PROBLEM) :
        lv_color_hex(COLOR_HEX_SELECTION), 0);

    lv_label_set_text(lblHomeProvider_, aiProviderToDisplayName(view_.provider));
    lv_label_set_text(lblHomeBigCounter_, std::to_string(view_.noteCount).c_str());

    if (lblHomeCounterDesc_) {
        if (view_.structuredStorageMode && !view_.littleFsMounted) {
            lv_label_set_text(lblHomeCounterDesc_, "LittleFS indisponible");
            lv_obj_set_style_text_color(lblHomeCounterDesc_, lv_color_hex(COLOR_HEX_PROBLEM), 0);
        } else if (view_.structuredStorageMode && !view_.repositoryReady) {
            lv_label_set_text(lblHomeCounterDesc_, "Journal endommagé");
            lv_obj_set_style_text_color(lblHomeCounterDesc_, lv_color_hex(COLOR_HEX_PROBLEM), 0);
        } else {
            lv_label_set_text(lblHomeCounterDesc_, (view_.noteCount == 1) ? "entrée en mémoire" : "entrées en mémoire");
            lv_obj_set_style_text_color(lblHomeCounterDesc_, lv_color_hex(COLOR_HEX_TXT_SEC), 0);
        }
    }

    if (timeService_ && timeService_->isTimeReliable()) {
        lv_label_set_text(lblHomeTime_, timeService_->formatLocalTime().c_str());
    } else {
        lv_label_set_text(lblHomeTime_, "--:--");
    }

    if (projService_ && projService_->getCurrentProjectId() > 0) {
        std::string pName = "[" + projService_->getCurrentProjectName() + "]";
        lv_label_set_text(lblHomeProject_, pName.c_str());
    } else {
        lv_label_set_text(lblHomeProject_, "");
    }

    if (lblHomeStatus_) {
        lv_label_set_text(lblHomeStatus_, view_.statusMessage.c_str());
        lv_obj_set_style_text_color(lblHomeStatus_,
            (view_.state == JournalState::ERROR_STATE) ? lv_color_hex(COLOR_HEX_PROBLEM) :
            (view_.state == JournalState::DISCONNECTED) ? lv_color_hex(COLOR_HEX_DUE) :
            lv_color_hex(COLOR_HEX_TXT_SEC), 0);
    }

    if (view_.state == JournalState::LISTENING) {
        lv_label_set_text(lblHomeTalk_, "TERMINER");
        lv_obj_set_style_bg_color(btnHomeTalk_, lv_color_hex(COLOR_HEX_PROBLEM), 0);
    } else {
        lv_label_set_text(lblHomeTalk_, "PARLER");
        lv_obj_set_style_bg_color(btnHomeTalk_, lv_color_hex(COLOR_HEX_ACTION), 0);
    }
}

void JournalUI::updateJournalWidgets() {
    if (journalCardObjs_.empty()) return;

    const JournalDisplaySnapshot snap = buildJournalSnapshot();

    Serial.println("[VJ][UI] Page=JOURNAL");
    Serial.printf("[VJ][UI] QueryResults=%u\n", static_cast<unsigned>(snap.queryResultCount));
    Serial.printf("[VJ][UI] VisibleCards=%u\n", static_cast<unsigned>(snap.visibleCards));

    if (lblJournalTitle_) {
        lv_label_set_text(lblJournalTitle_, snap.headerTitle.c_str());
    }
    if (lblJournalSubtitle_) {
        lv_label_set_text(lblJournalSubtitle_, snap.subtitle.c_str());
    }
    if (btnJournalClearFilter_) {
        if (snap.filterActive) {
            lv_obj_remove_flag(btnJournalClearFilter_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(btnJournalClearFilter_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (lblJournalEmpty_) {
        if (!snap.statusBanner.empty()) {
            lv_label_set_text(lblJournalEmpty_, snap.statusBanner.c_str());
            lv_obj_set_style_text_color(
                lblJournalEmpty_,
                snap.statusIsError ? lv_color_hex(COLOR_HEX_PROBLEM)
                                   : (snap.filterActive ? lv_color_hex(COLOR_HEX_DUE)
                                                        : lv_color_hex(COLOR_HEX_TXT_SEC)),
                0);
            lv_obj_remove_flag(lblJournalEmpty_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(lblJournalEmpty_, "");
            lv_obj_add_flag(lblJournalEmpty_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (size_t i = 0; i < journalCardObjs_.size(); i++) {
        lv_obj_t* card = journalCardObjs_[i];
        lv_obj_t* lblText = lv_obj_get_child(card, 0);
        lv_obj_t* lblMeta = lv_obj_get_child(card, 1);

        if (i < snap.cards.size() && snap.cards[i].visible) {
            const auto& cs = snap.cards[i];
            if (i < journalCardIds_.size()) {
                journalCardIds_[i] = cs.readError ? 0 : cs.entryId;
            }
            lv_obj_remove_flag(card, LV_OBJ_FLAG_HIDDEN);
            if (lblText) {
                lv_label_set_text(lblText, cs.text.c_str());
                lv_obj_set_style_text_color(
                    lblText,
                    cs.readError ? lv_color_hex(COLOR_HEX_PROBLEM)
                                 : lv_color_hex(COLOR_HEX_TXT_PRI),
                    0);
            }
            if (lblMeta) {
                lv_label_set_text(lblMeta, cs.meta.c_str());
            }
            if (cs.readError) {
                Serial.printf("[VJ][UI] ReadError ID=%u Reason=%s\n",
                              static_cast<unsigned>(cs.entryId),
                              cs.errorReason.c_str());
            }
        } else {
            if (i < journalCardIds_.size()) journalCardIds_[i] = 0;
            if (lblText) lv_label_set_text(lblText, "");
            if (lblMeta) lv_label_set_text(lblMeta, "");
            lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void JournalUI::updateProjectsWidgets() {
    if (!repo_ || !projService_ || projectCardObjs_.empty()) return;

    auto projects = projService_->listProjects(true);
    for (size_t i = 0; i < projectCardObjs_.size(); i++) {
        lv_obj_t* card = projectCardObjs_[i];
        if (i < projects.size()) {
            lv_obj_remove_flag(card, LV_OBJ_FLAG_HIDDEN);
            const auto& p = projects[i];
            if (i < projectCardIds_.size()) projectCardIds_[i] = p.id;

            lv_obj_t* lblName = lv_obj_get_child(card, 0);
            std::string nameStr = p.name;
            if (p.id == projService_->getCurrentProjectId()) {
                nameStr = "• " + nameStr + " (actif)";
            }
            if (lblName) lv_label_set_text(lblName, nameStr.c_str());

            lv_obj_t* lblMeta = lv_obj_get_child(card, 1);
            VoiceJournal::EntryFilter ef;
            ef.projectId = static_cast<int32_t>(p.id);
            size_t entryCount = repo_->queryEntryIds(ef).size();

            ef.type = static_cast<int16_t>(VoiceJournal::EntryType::PROBLEM);
            ef.status = static_cast<int16_t>(VoiceJournal::EntryStatus::OPEN);
            size_t probCount = repo_->queryEntryIds(ef).size();

            std::string meta = std::to_string(entryCount) + " entrées · " + std::to_string(probCount) + " problèmes";
            if (lblMeta) lv_label_set_text(lblMeta, meta.c_str());
        } else {
            if (i < projectCardIds_.size()) projectCardIds_[i] = 0;
            lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void JournalUI::updateTasksWidgets() {
    if (!taskService_ || taskCardObjs_.empty()) return;

    auto tasks = taskService_->listTasks(false, -1, 3);
    size_t openCount = taskService_->countOpenTasks();
    lv_label_set_text(lblTasksCount_, (std::to_string(openCount) + " à faire").c_str());

    for (size_t i = 0; i < taskCardObjs_.size(); i++) {
        lv_obj_t* card = taskCardObjs_[i];
        if (i < tasks.size()) {
            lv_obj_remove_flag(card, LV_OBJ_FLAG_HIDDEN);
            const auto& t = tasks[i];
            if (i < taskCardIds_.size()) taskCardIds_[i] = t.id;

            lv_obj_t* cb = lv_obj_get_child(card, 0);
            if (cb) {
                if (t.status == VoiceJournal::EntryStatus::COMPLETED) {
                    lv_obj_add_state(cb, LV_STATE_CHECKED);
                } else {
                    lv_obj_remove_state(cb, LV_STATE_CHECKED);
                }
            }

            lv_obj_t* lblDesc = lv_obj_get_child(card, 1);
            std::string prev = truncateUtf8Std(t.text, 28);
            if (prev.size() < t.text.size()) prev += "...";
            if (lblDesc) lv_label_set_text(lblDesc, prev.c_str());

            lv_obj_t* lblMeta = lv_obj_get_child(card, 2);
            std::string meta;
            if (timeService_ && t.dueAt > 0) {
                meta += timeService_->formatDisplayDate(t.dueAt) + " · ";
            }
            if (t.projectId > 0 && projService_) {
                VoiceJournal::Project p;
                if (projService_->getProject(t.projectId, p)) {
                    meta += p.name + " · ";
                }
            }
            meta += (t.status == VoiceJournal::EntryStatus::COMPLETED) ? "Terminée" : "À faire";
            if (lblMeta) lv_label_set_text(lblMeta, meta.c_str());
        } else {
            if (i < taskCardIds_.size()) taskCardIds_[i] = 0;
            lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void JournalUI::updateDetailWidgets() {
    if (!repo_ || selectedDetailEntryId_ == 0 || !lblDetailText_) return;

    VoiceJournal::JournalEntry entry;
    if (!repo_->getEntry(selectedDetailEntryId_, entry)) {
        setPage(UiPage::JOURNAL);
        return;
    }

    std::string title = std::string(VoiceJournal::entryTypeToFrench(entry.type)) + " #" + std::to_string(entry.id);
    lv_label_set_text(lblDetailTitle_, title.c_str());
    lv_label_set_text(lblDetailText_, entry.text.c_str());

    std::string pStr = "Projet : ";
    if (entry.projectId > 0 && projService_) {
        VoiceJournal::Project p;
        if (projService_->getProject(entry.projectId, p)) pStr += p.name;
        else pStr += "Inconnu";
    } else {
        pStr += "Aucun";
    }
    lv_label_set_text(lblDetailMetaProject_, pStr.c_str());

    std::string dStr = "Date : ";
    if (timeService_ && entry.createdAt > 0) {
        dStr += timeService_->formatDisplayDate(entry.createdAt) + " " + timeService_->formatLocalTime(entry.createdAt);
    } else {
        dStr += "Inconnue";
    }
    lv_label_set_text(lblDetailMetaDate_, dStr.c_str());

    std::string sStr = "Statut : " + std::string(VoiceJournal::entryStatusToFrench(entry.status));
    lv_label_set_text(lblDetailMetaStatus_, sStr.c_str());

    // Ajuster le bouton d'action contextuel
    if (entry.type == VoiceJournal::EntryType::PROBLEM) {
        lv_obj_remove_flag(btnDetailAction_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lblDetailAction_, (entry.status == VoiceJournal::EntryStatus::RESOLVED) ? "RÉOUVRIR" : "RÉSOLU");
    } else if (entry.type == VoiceJournal::EntryType::TASK) {
        lv_obj_remove_flag(btnDetailAction_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lblDetailAction_, (entry.status == VoiceJournal::EntryStatus::COMPLETED) ? "ROUVRIR" : "TERMINER");
    } else {
        lv_obj_add_flag(btnDetailAction_, LV_OBJ_FLAG_HIDDEN);
    }
}

void JournalUI::updateVoiceBanner() {
    if (!voiceBanner_) return;

    bool isVoiceActive = (view_.state == JournalState::LISTENING ||
                          view_.state == JournalState::TRANSCRIBING ||
                          view_.state == JournalState::PROCESSING_COMMAND ||
                          view_.state == JournalState::SPEAKING ||
                          view_.state == JournalState::ANALYZING);

    if (isVoiceActive) {
        lv_obj_remove_flag(voiceBanner_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(navBar_, LV_OBJ_FLAG_HIDDEN);

        const char* msg = (view_.state == JournalState::LISTENING) ? "ÉCOUTE EN COURS..." :
                          (view_.state == JournalState::TRANSCRIBING) ? "TRANSCRIPTION..." :
                          (view_.state == JournalState::ANALYZING) ? "ANALYSE IA..." :
                          (view_.state == JournalState::SPEAKING) ? "REPONSE AUDIO..." : "TRAITEMENT...";
        lv_label_set_text(voiceBannerStateLabel_, msg);
    } else {
        lv_obj_add_flag(voiceBanner_, LV_OBJ_FLAG_HIDDEN);
        if (currentPage_ != UiPage::DETAIL) {
            lv_obj_remove_flag(navBar_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void JournalUI::updateNavBar() {
    if (!btnNavHome_) return;
    auto setBtnHighlight = [](lv_obj_t* btn, bool active) {
        lv_obj_set_style_bg_color(btn, active ? lv_color_hex(COLOR_HEX_ACTION) : lv_color_hex(COLOR_HEX_CARD), 0);
        lv_obj_t* lbl = lv_obj_get_child(btn, 0);
        if (lbl) {
            lv_obj_set_style_text_color(lbl, active ? lv_color_hex(COLOR_HEX_TXT_PRI) : lv_color_hex(COLOR_HEX_TXT_SEC), 0);
        }
    };

    setBtnHighlight(btnNavHome_, currentPage_ == UiPage::HOME);
    setBtnHighlight(btnNavJournal_, currentPage_ == UiPage::JOURNAL);
    setBtnHighlight(btnNavProjects_, currentPage_ == UiPage::PROJECTS);
    setBtnHighlight(btnNavTasks_, currentPage_ == UiPage::TASKS);
}

#endif // ARDUINO

void JournalUI::begin(const JournalViewModel& initialView) {
    view_ = initialView;
    needsRedraw_ = true;

#ifdef ARDUINO
    buildLvglUi();
    setPage(UiPage::HOME);
#endif
}

void JournalUI::updateView(const JournalViewModel& view) {
    view_ = view;
    needsRedraw_ = true;
}

void JournalUI::renderIfNeeded() {
#ifdef ARDUINO
#if !VJ_DIAG_ENABLE_LVGL
    if (needsRedraw_) {
        M5.Display.fillRect(0, 60, 320, 180, COLOR_HEX_BG);
        M5.Display.setTextSize(2);
        M5.Display.setTextColor(COLOR_HEX_TXT_PRI, COLOR_HEX_BG);
        M5.Display.setCursor(10, 70);
        M5.Display.printf("Etat: %s\n", journalStateToUiLabel(view_.state));
        M5.Display.setCursor(10, 100);
        M5.Display.printf("IA: %s\n", aiProviderToDisplayName(view_.provider));
        M5.Display.setTextSize(1);
        M5.Display.setCursor(10, 136);
        M5.Display.println(toDisplayAscii(view_.statusMessage).c_str());
        needsRedraw_ = false;
    }
    return;
#endif
    if (needsRedraw_) {
        switch (currentPage_) {
            case UiPage::HOME:     updateHomeWidgets(); break;
            case UiPage::JOURNAL:  updateJournalWidgets(); break;
            case UiPage::PROJECTS: updateProjectsWidgets(); break;
            case UiPage::TASKS:    updateTasksWidgets(); break;
            case UiPage::DETAIL:   updateDetailWidgets(); break;
            default: break;
        }
        updateVoiceBanner();
        updateNavBar();
        needsRedraw_ = false;
    }
    lv_timer_handler();
#else
    needsRedraw_ = false;
#endif
}

void JournalUI::setPage(UiPage page) {
    currentPage_ = page;
    needsRedraw_ = true;

#ifdef ARDUINO
    if (!screenHome_) return;
    lv_obj_add_flag(screenHome_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(screenJournal_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(screenProjects_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(screenTasks_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(screenDetail_, LV_OBJ_FLAG_HIDDEN);

    switch (page) {
        case UiPage::HOME:
            lv_obj_remove_flag(screenHome_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(navBar_, LV_OBJ_FLAG_HIDDEN);
            updateHomeWidgets();
            break;
        case UiPage::JOURNAL:
            lv_obj_remove_flag(screenJournal_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(navBar_, LV_OBJ_FLAG_HIDDEN);
            updateJournalWidgets();
            break;
        case UiPage::PROJECTS:
            lv_obj_remove_flag(screenProjects_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(navBar_, LV_OBJ_FLAG_HIDDEN);
            updateProjectsWidgets();
            break;
        case UiPage::TASKS:
            lv_obj_remove_flag(screenTasks_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(navBar_, LV_OBJ_FLAG_HIDDEN);
            updateTasksWidgets();
            break;
        case UiPage::DETAIL:
            lv_obj_remove_flag(screenDetail_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(navBar_, LV_OBJ_FLAG_HIDDEN);
            updateDetailWidgets();
            break;
        default:
            break;
    }
    updateNavBar();
#endif
}

void JournalUI::showEntryDetail(uint32_t entryId) {
    selectedDetailEntryId_ = entryId;
    setPage(UiPage::DETAIL);
}

void JournalUI::setJournalFilter(int32_t projectId, int16_t type, const String& title) {
    filterProjectId_ = projectId;
    filterType_ = type;
    filterTitle_ = title;
    setPage(UiPage::JOURNAL);
}

void JournalUI::clearJournalFilter() {
    filterProjectId_ = -1;
    filterType_ = -1;
    filterTitle_ = "";
    needsRedraw_ = true;
#ifdef ARDUINO
    if (currentPage_ == UiPage::JOURNAL) {
        updateJournalWidgets();
    }
#endif
}

void JournalUI::refreshJournal() {
    needsRedraw_ = true;
#ifdef ARDUINO
    if (currentPage_ == UiPage::JOURNAL) {
        updateJournalWidgets();
    }
#endif
}

std::string JournalUI::truncateUtf8Std(const std::string& utf8, size_t maxBytes) {
    if (utf8.size() <= maxBytes) {
        return utf8;
    }
    size_t cut = maxBytes;
    // Reculer si on coupe au milieu d'une séquence multi-octets UTF-8 (octets de continuation 10xxxxxx)
    while (cut > 0 && (static_cast<uint8_t>(utf8[cut]) & 0xC0u) == 0x80u) {
        --cut;
    }
    return utf8.substr(0, cut);
}

JournalDisplaySnapshot JournalUI::buildJournalSnapshot() const {
    JournalDisplaySnapshot snap;
    snap.filterActive = hasActiveJournalFilter();
    snap.headerTitle = (snap.filterActive && filterTitle_.length() > 0)
                           ? truncateUtf8Std(std::string(filterTitle_.c_str()), 18)
                           : "Journal";
    snap.cards.resize(3);

    if (!repo_) {
        snap.littleFsMounted = false;
        snap.repositoryReady = false;
        snap.subtitle = "Indisponible";
        snap.statusBanner = "Stockage LittleFS indisponible";
        snap.statusIsError = true;
        return snap;
    }

    snap.littleFsMounted = repo_->isFileSystemMounted();
    snap.repositoryReady = repo_->isInitialized();

    if (!snap.littleFsMounted) {
        snap.subtitle = "LittleFS OFF";
        snap.statusBanner = "LittleFS indisponible (non monté)";
        snap.statusIsError = true;
        return snap;
    }

    if (!snap.repositoryReady) {
        snap.subtitle = "Erreur";
        std::string err = repo_->getLastError();
        snap.statusBanner = err.empty() ? "Journal endommagé" : ("Journal endommagé : " + err);
        snap.statusIsError = true;
        return snap;
    }

    snap.totalEntries = repo_->countEntries();
    snap.subtitle = std::to_string(snap.totalEntries) +
                    ((snap.totalEntries == 1) ? " entrée" : " entrées");

    VoiceJournal::EntryFilter filter;
    filter.projectId = filterProjectId_;
    filter.type = filterType_;
    const auto ids = repo_->queryEntryIds(filter, 3, 0);
    snap.queryResultCount = ids.size();

    if (snap.totalEntries == 0) {
        snap.statusBanner = "Journal vide";
        snap.statusIsError = false;
        return snap;
    }

    if (ids.empty()) {
        snap.statusBanner = snap.filterActive
                                ? "Aucun résultat pour ce filtre"
                                : "Journal vide";
        snap.statusIsError = false;
        return snap;
    }

    for (size_t i = 0; i < snap.cards.size(); ++i) {
        auto& cs = snap.cards[i];
        if (i < ids.size()) {
            cs.entryId = ids[i];
            cs.visible = true;
            VoiceJournal::JournalEntry entry;
            if (repo_->getEntry(ids[i], entry)) {
                cs.readError = false;
                std::string prev = truncateUtf8Std(entry.text, 34);
                if (prev.size() < entry.text.size()) prev += "...";
                cs.text = prev;

                std::string meta;
                if (entry.projectId > 0 && projService_) {
                    VoiceJournal::Project p;
                    if (projService_->getProject(entry.projectId, p)) {
                        meta += p.name + " · ";
                    }
                }
                if (timeService_ && entry.createdAt > 0) {
                    meta += timeService_->formatDisplayDate(entry.createdAt) + " · ";
                }
                meta += VoiceJournal::entryTypeToFrench(entry.type);
                cs.meta = meta;
                ++snap.visibleCards;
            } else {
                cs.readError = true;
                cs.errorReason = repo_->getLastError();
                cs.text = "Erreur lecture entrée #" + std::to_string(ids[i]);
                cs.meta = cs.errorReason.empty() ? "Erreur stockage" : cs.errorReason;
                ++snap.readErrors;
                ++snap.visibleCards;
            }
        } else {
            cs.entryId = 0;
            cs.visible = false;
        }
    }

    return snap;
}

bool JournalUI::isTalkButtonEnabled() const {
    return (view_.state == JournalState::READY || view_.state == JournalState::LISTENING);
}

bool JournalUI::isProviderSwitchEnabled() const {
    return (view_.state == JournalState::READY ||
            view_.state == JournalState::DISCONNECTED ||
            view_.state == JournalState::ERROR_STATE);
}

bool JournalUI::isInsideRect(int x, int y, int rx, int ry, int rw, int rh) {
    return (x >= rx && x <= (rx + rw) && y >= ry && y <= (ry + rh));
}

TouchAction JournalUI::pollTouch(uint32_t nowMs) {
#ifdef ARDUINO
    if (pendingAction_ != TouchAction::NONE) {
        TouchAction act = pendingAction_;
        pendingAction_ = TouchAction::NONE;
        return act;
    }

#if !VJ_DIAG_ENABLE_LVGL
    if (M5.Touch.getCount() > 0) {
        auto detail = M5.Touch.getDetail(0);
        TouchSample sample;
        sample.isPressed = detail.isPressed();
        sample.wasPressed = detail.wasPressed();
        sample.x = detail.x;
        sample.y = detail.y;
        return processTouchSample(sample, nowMs);
    }
    return processTouchSample(TouchSample{}, nowMs);
#else
    (void)nowMs;
    return TouchAction::NONE;
#endif
#else
    (void)nowMs;
    return TouchAction::NONE;
#endif
}

TouchAction JournalUI::processTouchSample(const TouchSample& sample, uint32_t nowMs) {
    // 1. Aucun contact actif : réinitialiser le verrou de maintien
    if (!sample.isPressed && !sample.wasPressed) {
        touchDown_ = false;
        return TouchAction::NONE;
    }

    // 2. Contact physique déjà actif (doigt maintenu) : ne jamais régénérer d'événement
    if (touchDown_) {
        if (!sample.isPressed) {
            touchDown_ = false;
        }
        return TouchAction::NONE;
    }

    touchDown_ = sample.isPressed;

    // 3. Bouton STOP examiné EN PREMIER, indépendant de l'anti-rebond de PARLER,
    //    et disponible dans tous les états actifs (LISTENING, TRANSCRIBING, ANALYZING, SPEAKING, etc.)
    if (isInsideRect(sample.x, sample.y, BTN_STOP_X, BTN_Y, BTN_STOP_W, BTN_H)) {
        return TouchAction::STOP;
    }

    // 4. Bouton PARLER / TERMINER : état compatible + anti-rebond dédié (TOUCH_DEBOUNCE_MS)
    if (isInsideRect(sample.x, sample.y, BTN_TALK_X, BTN_Y, BTN_TALK_W, BTN_H)) {
        if (!isTalkButtonEnabled()) {
            return TouchAction::NONE;
        }
        if (hasLastTalkAccepted_ && (nowMs - lastTalkAcceptedMs_) < AppConfig::TOUCH_DEBOUNCE_MS) {
            return TouchAction::NONE;
        }
        hasLastTalkAccepted_ = true;
        lastTalkAcceptedMs_ = nowMs;
        return TouchAction::TALK_TOGGLE;
    }

    // 5. Bouton sélecteur de fournisseur IA [OPENAI / GEMINI]
    if (isInsideRect(sample.x, sample.y,
                     BTN_PROVIDER_X, BTN_PROVIDER_Y, BTN_PROVIDER_W, BTN_PROVIDER_H)) {
        if (!isProviderSwitchEnabled()) {
            return TouchAction::NONE;
        }
        if (hasLastProviderAccepted_ && (nowMs - lastProviderAcceptedMs_) < AppConfig::PROVIDER_SWITCH_DEBOUNCE_MS) {
            return TouchAction::NONE;
        }
        hasLastProviderAccepted_ = true;
        lastProviderAcceptedMs_ = nowMs;
        return TouchAction::SWITCH_PROVIDER;
    }

    return TouchAction::NONE;
}

String JournalUI::toDisplayAscii(const String& utf8) {
    String out = "";
    size_t i = 0;
    while (i < utf8.length()) {
        uint8_t c = static_cast<uint8_t>(utf8[i]);
        if (c < 0x80) {
            out += static_cast<char>(c);
            i++;
        } else if (c == 0xC3 && i + 1 < utf8.length()) {
            uint8_t c2 = static_cast<uint8_t>(utf8[i + 1]);
            switch (c2) {
                case 0xA9: out += 'e'; break; // é
                case 0xA8: out += 'e'; break; // è
                case 0xAA: out += 'e'; break; // ê
                case 0xAB: out += 'e'; break; // ë
                case 0xA0: out += 'a'; break; // à
                case 0xA2: out += 'a'; break; // â
                case 0xA7: out += 'c'; break; // ç
                case 0xAE: out += 'i'; break; // î
                case 0xAF: out += 'i'; break; // ï
                case 0xB4: out += 'o'; break; // ô
                case 0xB9: out += 'u'; break; // ù
                case 0xBB: out += 'u'; break; // û
                case 0x89: out += 'E'; break; // É
                case 0x88: out += 'E'; break; // È
                default:   out += '?'; break;
            }
            i += 2;
        } else {
            out += '?';
            i++;
        }
    }
    return out;
}
