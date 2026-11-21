#pragma once

#include <cstdint>
#include <vector>
#include "VoiceController.h"

/**
 * @brief Mock deterministe de la facade VoiceKit pour tester VoiceController hors materiel.
 */
class MockVoiceEngine final : public IVoiceEngine {
public:
    void update() override {
        ++updateCalls;
    }

    bool listen() override {
        ++listenCalls;
        if (!ready || busy || listening || transcribing || rejectListen) {
            lastError = "Refus listen()";
            return false;
        }
        listening = true;
        busy = true;
        state = VoiceEngineState::LISTENING;
        activeTurnId = ++nextTurnId;
        lastTurnId = activeTurnId;
        if (onStateChangedCb) onStateChangedCb(state);
        if (onListeningStartedCb) onListeningStartedCb(activeTurnId);
        return true;
    }

    bool stopListening() override {
        ++stopListeningCalls;
        if (!listening) return false;
        listening = false;
        transcribing = true;
        pendingTurnId = activeTurnId;
        activeTurnId = 0;
        state = VoiceEngineState::TRANSCRIBING;
        if (onListeningFinishedCb) onListeningFinishedCb();
        if (onStateChangedCb) onStateChangedCb(state);
        return true;
    }

    void cancelListening() override {
        ++cancelListeningCalls;
        listening = false;
        transcribing = false;
        activeTurnId = 0;
        pendingTurnId = 0;
        if (deferReadyAfterCancel) {
            busy = true;
            state = VoiceEngineState::STOPPED;
            if (onStateChangedCb) onStateChangedCb(state);
            return;
        }
        busy = false;
        if (ready) {
            state = VoiceEngineState::READY;
            if (onStateChangedCb) onStateChangedCb(state);
        }
    }

    bool say(const String& text) override {
        ++sayCalls;
        if (!ready || busy || failSay) {
            lastError = "Echec say()";
            return false;
        }
        spokenTexts.push_back(text);
        busy = true;
        state = VoiceEngineState::SPEAKING;
        if (onStateChangedCb) onStateChangedCb(state);
        if (onSpeechStartedCb) onSpeechStartedCb();
        return true;
    }

    bool askAi(const String& prompt) override {
        ++askAiCalls;
        if (!ready || busy || failAskAi) {
            lastError = "Echec askAi()";
            return false;
        }
        aiPrompts.push_back(prompt);
        busy = true;
        state = VoiceEngineState::GENERATING;
        if (onStateChangedCb) onStateChangedCb(state);
        return true;
    }

    void stop() override {
        ++stopCalls;
        listening = false;
        transcribing = false;
        busy = false;
        activeTurnId = 0;
        pendingTurnId = 0;
        if (ready) {
            state = VoiceEngineState::READY;
            if (onStateChangedCb) onStateChangedCb(state);
        }
    }

    bool isReady() const override { return ready && !busy && !listening && !transcribing; }
    bool isBusy() const override { return busy || listening || transcribing; }
    bool isListening() const override { return listening; }
    bool isTranscribing() const override { return transcribing; }
    VoiceEngineState getState() const override { return state; }
    String getLastError() const override { return lastError; }

    uint32_t getActiveCaptureTurnId() const override { return activeTurnId; }
    uint32_t getPendingTranscriptionTurnId() const override { return pendingTurnId; }
    uint32_t getLastCaptureTurnId() const override { return lastTurnId; }

    void onReady(VoidCb cb) override { onReadyCb = std::move(cb); }
    void onError(ErrorCb cb) override { onErrorCb = std::move(cb); }
    void onStateChanged(StateCb cb) override { onStateChangedCb = std::move(cb); }
    void onListeningStartedWithTurnId(TurnCb cb) override { onListeningStartedCb = std::move(cb); }
    void onListeningFinished(VoidCb cb) override { onListeningFinishedCb = std::move(cb); }
    void onTranscriptionWithTurnId(TranscriptionCb cb) override { onTranscriptionCb = std::move(cb); }
    void onAssistantText(AssistantTextCb cb) override { onAssistantTextCb = std::move(cb); }
    void onSpeechStarted(VoidCb cb) override { onSpeechStartedCb = std::move(cb); }
    void onSpeechFinished(VoidCb cb) override { onSpeechFinishedCb = std::move(cb); }

    // Helpers de simulation d'evenements VoiceKit
    void simulateReady() {
        ready = true;
        busy = false;
        listening = false;
        transcribing = false;
        state = VoiceEngineState::READY;
        if (onStateChangedCb) onStateChangedCb(state);
        if (onReadyCb) onReadyCb();
    }

    void simulateDisconnect() {
        ready = false;
        busy = false;
        listening = false;
        transcribing = false;
        state = VoiceEngineState::DISCONNECTED;
        if (onStateChangedCb) onStateChangedCb(state);
    }

    void simulateTranscription(const String& text, bool isFinal, uint32_t turnId) {
        if (isFinal) {
            transcribing = false;
            busy = false;
            pendingTurnId = 0;
            if (ready) {
                state = VoiceEngineState::READY;
            }
        }
        if (onTranscriptionCb) {
            onTranscriptionCb(text, isFinal, turnId);
        }
    }

    void simulateSpeechFinished() {
        busy = false;
        if (ready) {
            state = VoiceEngineState::READY;
        }
        if (onSpeechFinishedCb) onSpeechFinishedCb();
        if (onStateChangedCb) onStateChangedCb(state);
    }

    void simulateAssistantText(const String& text) {
        if (onAssistantTextCb) onAssistantTextCb(text);
    }

    void simulateSpeechStarted() {
        busy = true;
        state = VoiceEngineState::SPEAKING;
        if (onStateChangedCb) onStateChangedCb(state);
        if (onSpeechStartedCb) onSpeechStartedCb();
    }

    bool ready = false;
    bool busy = false;
    bool listening = false;
    bool transcribing = false;
    bool rejectListen = false;
    bool failSay = false;
    bool failAskAi = false;
    bool deferReadyAfterCancel = false;

    VoiceEngineState state = VoiceEngineState::UNINITIALIZED;
    String lastError;

    uint32_t nextTurnId = 0;
    uint32_t activeTurnId = 0;
    uint32_t pendingTurnId = 0;
    uint32_t lastTurnId = 0;

    int updateCalls = 0;
    int listenCalls = 0;
    int stopListeningCalls = 0;
    int cancelListeningCalls = 0;
    int sayCalls = 0;
    int askAiCalls = 0;
    int stopCalls = 0;

    std::vector<String> spokenTexts;
    std::vector<String> aiPrompts;

    VoidCb onReadyCb;
    ErrorCb onErrorCb;
    StateCb onStateChangedCb;
    TurnCb onListeningStartedCb;
    VoidCb onListeningFinishedCb;
    TranscriptionCb onTranscriptionCb;
    AssistantTextCb onAssistantTextCb;
    VoidCb onSpeechStartedCb;
    VoidCb onSpeechFinishedCb;
};
