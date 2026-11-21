#pragma once

#include <Arduino.h>
#include <cstdint>
#include <deque>
#include <functional>

#include "AppConfig.h"
#include "CommandRouter.h"
#include "JournalStorage.h"

#ifdef ARDUINO
#include <VoiceKit.h>
#endif

/**
 * @brief Etat normalise expose par l'adaptateur moteur vocal (mappe directement VoiceState).
 */
enum class VoiceEngineState : uint8_t {
    UNINITIALIZED = 0,
    IDLE,
    CONNECTING_WIFI,
    CONNECTING_SERVICE,
    READY,
    GENERATING,
    SPEAKING,
    LISTENING,
    TRANSCRIBING,
    ERROR_STATE,
    STOPPED,
    DISCONNECTED
};

/**
 * @brief Interface mince d'abstraction de la facade publique VoiceKit.
 *        En production, VoiceKitEngineAdapter delegue 1-a-1 a VoiceKit.
 *        En tests natifs, un mock simule les evenements et identifiants de tour.
 */
class IVoiceEngine {
public:
    using VoidCb          = std::function<void()>;
    using ErrorCb         = std::function<void(const String&)>;
    using StateCb         = std::function<void(VoiceEngineState)>;
    using TurnCb          = std::function<void(uint32_t)>;
    using TranscriptionCb = std::function<void(const String&, bool, uint32_t)>;
    using AssistantTextCb = std::function<void(const String&)>;

    virtual ~IVoiceEngine() = default;

    virtual void update() = 0;
    virtual bool listen() = 0;
    virtual bool stopListening() = 0;
    virtual void cancelListening() = 0;
    virtual bool say(const String& text) = 0;
    virtual bool askAi(const String& prompt) = 0;
    virtual void stop() = 0;

    virtual bool isReady() const = 0;
    virtual bool isBusy() const = 0;
    virtual bool isListening() const = 0;
    virtual bool isTranscribing() const = 0;
    virtual VoiceEngineState getState() const = 0;
    virtual String getLastError() const = 0;

    virtual uint32_t getActiveCaptureTurnId() const = 0;
    virtual uint32_t getPendingTranscriptionTurnId() const = 0;
    virtual uint32_t getLastCaptureTurnId() const = 0;

    virtual void onReady(VoidCb cb) = 0;
    virtual void onError(ErrorCb cb) = 0;
    virtual void onStateChanged(StateCb cb) = 0;
    virtual void onListeningStartedWithTurnId(TurnCb cb) = 0;
    virtual void onListeningFinished(VoidCb cb) = 0;
    virtual void onTranscriptionWithTurnId(TranscriptionCb cb) = 0;
    virtual void onAssistantText(AssistantTextCb cb) = 0;
    virtual void onSpeechStarted(VoidCb cb) = 0;
    virtual void onSpeechFinished(VoidCb cb) = 0;
};

#ifdef ARDUINO
/**
 * @brief Adaptateur de production deleguant directement a la facade publique <VoiceKit.h>.
 */
class VoiceKitEngineAdapter final : public IVoiceEngine {
public:
    explicit VoiceKitEngineAdapter(VoiceKit& voiceKit) : voice_(voiceKit) {}

    bool begin(const VoiceKitConfig& cfg) { return voice_.begin(cfg); }
    void update() override { voice_.update(); }
    bool listen() override { return voice_.listen(); }
    bool stopListening() override { return voice_.stopListening(); }
    void cancelListening() override { voice_.cancelListening(); }
    bool say(const String& text) override { return voice_.say(text); }
    bool askAi(const String& prompt) override { return voice_.askAi(prompt); }
    void stop() override { voice_.stop(); }

    bool isReady() const override { return voice_.isReady(); }
    bool isBusy() const override { return voice_.isBusy(); }
    bool isListening() const override { return voice_.isListening(); }
    bool isTranscribing() const override { return voice_.isTranscribing(); }
    VoiceEngineState getState() const override { return mapState(voice_.getState()); }
    String getLastError() const override { return voice_.getLastError(); }

    uint32_t getActiveCaptureTurnId() const override { return voice_.getActiveCaptureTurnId(); }
    uint32_t getPendingTranscriptionTurnId() const override { return voice_.getPendingTranscriptionTurnId(); }
    uint32_t getLastCaptureTurnId() const override { return voice_.getLastCaptureTurnId(); }

    void onReady(VoidCb cb) override { voice_.onReady(std::move(cb)); }
    void onError(ErrorCb cb) override { voice_.onError(std::move(cb)); }
    void onStateChanged(StateCb cb) override {
        voice_.onStateChanged([cb = std::move(cb)](VoiceState s) {
            if (cb) cb(mapState(s));
        });
    }
    void onListeningStartedWithTurnId(TurnCb cb) override {
        voice_.onListeningStartedWithTurnId(std::move(cb));
    }
    void onListeningFinished(VoidCb cb) override { voice_.onListeningFinished(std::move(cb)); }
    void onTranscriptionWithTurnId(TranscriptionCb cb) override {
        voice_.onTranscriptionWithTurnId(std::move(cb));
    }
    void onAssistantText(AssistantTextCb cb) override { voice_.onAssistantText(std::move(cb)); }
    void onSpeechStarted(VoidCb cb) override { voice_.onSpeechStarted(std::move(cb)); }
    void onSpeechFinished(VoidCb cb) override {
        VoiceKit::VoidCallback wrapped = std::move(cb);
        voice_.onSpeechFinished(std::move(wrapped));
    }

    static VoiceEngineState mapState(VoiceState s) {
        switch (s) {
            case VoiceState::UNINITIALIZED:      return VoiceEngineState::UNINITIALIZED;
            case VoiceState::IDLE:               return VoiceEngineState::IDLE;
            case VoiceState::CONNECTING_WIFI:    return VoiceEngineState::CONNECTING_WIFI;
            case VoiceState::CONNECTING_SERVICE: return VoiceEngineState::CONNECTING_SERVICE;
            case VoiceState::READY:              return VoiceEngineState::READY;
            case VoiceState::GENERATING:         return VoiceEngineState::GENERATING;
            case VoiceState::SPEAKING:           return VoiceEngineState::SPEAKING;
            case VoiceState::LISTENING:          return VoiceEngineState::LISTENING;
            case VoiceState::TRANSCRIBING:       return VoiceEngineState::TRANSCRIBING;
            case VoiceState::ERROR_STATE:        return VoiceEngineState::ERROR_STATE;
            case VoiceState::STOPPED:            return VoiceEngineState::STOPPED;
            case VoiceState::DISCONNECTED:       return VoiceEngineState::DISCONNECTED;
            default:                             return VoiceEngineState::ERROR_STATE;
        }
    }

private:
    VoiceKit& voice_;
};
#endif

/**
 * @brief Instantane d'etat publie par VoiceController vers l'interface utilisateur.
 */
struct JournalViewModel {
    JournalState state = JournalState::INITIALIZING;
    size_t noteCount = 0;
    String lastTranscript;
    String statusMessage;
    bool connected = false;
    bool clearConfirmPending = false;
};

/**
 * @class VoiceController
 * @brief Controleur applicatif principal de VoiceJournal v0.1.0.
 *
 * Responsabilites :
 * - Machine a etats applicative explicite (JournalState).
 * - Correlation stricte des tours de capture STT par turnId.
 * - Orchestration deterministe des commandes (CommandRouter -> JournalStorage).
 * - File d'attente TTS courte et non bloquante.
 * - Construction bornee et securisee de la requete d'analyse Gemini (askAi).
 * - Gestion prioritaire et idempotente de STOP.
 * - Watchdogs non bloquants (duree micro, timeout transcription, timeout IA/TTS).
 */
class VoiceController {
public:
    using ViewUpdateCallback = std::function<void(const JournalViewModel&)>;

    VoiceController(IVoiceEngine& engine, JournalStorage& storage, CommandRouter& router);

    /**
     * @brief Enregistre les callbacks aupres du moteur vocal et initialise la machine a etats.
     */
    void attachCallbacks();

    /**
     * @brief Traitement periodique non bloquant a appeler dans loop().
     * @param nowMs Horodatage courant en millisecondes (millis()).
     */
    void update(uint32_t nowMs);

    /**
     * @brief Action utilisateur sur le bouton principal [PARLER / TERMINER].
     * @return true si l'action a ete acceptee, false si ignoree (etat incompatible / occupe).
     */
    bool onTalkButtonPressed(uint32_t nowMs);

    /**
     * @brief Action prioritaire sur le bouton [STOP].
     *        Coupe ecoute, TTS ou requete IA, invalide le tour et la confirmation en attente,
     *        et preserve integralement les notes NVS.
     */
    void onStopButtonPressed(uint32_t nowMs);

    /**
     * @brief Notifie un echec d'initialisation moteur.
     */
    void notifyInitError(const String& reason);

    /**
     * @brief Abonne l'interface utilisateur aux mises a jour de vue.
     */
    void onViewUpdated(ViewUpdateCallback cb) { onViewUpdated_ = std::move(cb); }

    JournalState getState() const { return state_; }
    const JournalViewModel& getViewModel() const { return view_; }

    /**
     * @brief Construit la requete d'analyse IA bornee avec separation donnees/instructions.
     */
    String buildAnalysisPrompt() const;

    /**
     * @brief Indique si une requete IA est actuellement active.
     */
    bool isAiAnalysisActive() const { return aiRequestActive_; }

    /**
     * @brief Nombre d'elements en attente dans la file TTS locale.
     */
    size_t pendingSpeechCount() const { return ttsQueue_.size(); }

private:
    void setState(JournalState newState);
    void setStatusMessage(const String& msg);
    void setTranscriptDisplay(const String& text);
    void publishView();

    bool startListeningSession(uint32_t nowMs);
    bool finishListeningSession(uint32_t nowMs);
    void processFinalTranscription(const String& text, uint32_t nowMs);
    void executeCommand(const CommandResult& cmd, uint32_t nowMs);

    bool enqueueSpeech(const String& text);
    void pumpSpeechQueue(uint32_t nowMs);
    void handleWatchdogs(uint32_t nowMs);
    void invalidateCurrentTurn();

    static String formatCountSpeech(size_t count);

    IVoiceEngine& engine_;
    JournalStorage& storage_;
    CommandRouter& router_;

    JournalState state_ = JournalState::INITIALIZING;
    JournalViewModel view_;
    ViewUpdateCallback onViewUpdated_;

    // Correlation stricte des tours STT
    bool captureSessionOpen_ = false;
    uint32_t expectedTurnId_ = 0;
    uint32_t lastProcessedTurnId_ = 0;
    uint32_t invalidatedTurnId_ = 0;

    // Transcription finale differee pour traitement hors callback
    bool hasPendingFinal_ = false;
    String pendingFinalText_;

    // File TTS courte (evite tout appel say() quand le moteur est occupe)
    std::deque<String> ttsQueue_;

    // Suivi des requetes IA et watchdogs
    bool aiRequestActive_ = false;
    uint32_t stateEnteredAtMs_ = 0;
    uint32_t captureStartedAtMs_ = 0;
};
