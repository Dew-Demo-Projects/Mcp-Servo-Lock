#include <EEPROM.h>
#include "LockController.h"

LockController::LockController()
    : unlockStartTime(0), alarmStartTime(0),
      activeFeedback(FB_NONE), feedbackStartTime(0),
      logHead(0), logCount(0) {
    currentState = STATE_LOCKED;
    currentMode = MODE_AUTO;
    isAlarmActive = false;
    wrongAttemptCount = 0;

    // Default configuration
    strncpy(correctPIN, "1234", sizeof(correctPIN) - 1);
    correctPIN[sizeof(correctPIN) - 1] = '\0';
    config.autoLockTimeout = 5000;
    config.alarmTimeout = 10000;
    config.wrongCodeThreshold = 3;

    memset(logQueue, 0, sizeof(logQueue));
}

void LockController::begin() {
    eepromLoadConfig();
    addLog("SYSTEM_INIT");
}

void LockController::update() {
    const unsigned long now = millis();

    // Manage feedback duration
    if (activeFeedback != FB_NONE) {
        if (now - feedbackStartTime >= feedbackDuration()) {
            activeFeedback = FB_NONE;
        }
    }

    // Auto-lock timer
    if (currentState == STATE_UNLOCKED && currentMode == MODE_AUTO) {
        if (now - unlockStartTime >= config.autoLockTimeout) {
            performLock("AUTO");
        }
    }

    // Alarm timeout
    if (isAlarmActive) {
        if (now - alarmStartTime >= config.alarmTimeout) {
            clearAlarm();
        }
    }
}

// --- HTTP Actions ---

void LockController::remoteLock() {
    if (isAlarmActive) return;
    performLock("REMOTE");
    startFeedback(FB_REMOTE_LOCK);
}

void LockController::remoteUnlock() {
    if (isAlarmActive) return;
    performUnlock("REMOTE");
    startFeedback(FB_REMOTE_UNLOCK);
}

void LockController::toggleMode() {
    currentMode = (currentMode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
    addLog(currentMode == MODE_AUTO ? "MODE_AUTO" : "MODE_MANUAL");
    eepromSaveConfig();
    startFeedback(FB_CONFIG);
}

void LockController::setAutoLockTimeout(const unsigned long ms) {
    config.autoLockTimeout = ms;
    addLog("CFG_TIMEOUT");
    eepromSaveConfig();
    startFeedback(FB_CONFIG);
}

void LockController::setWrongCodeThreshold(const uint8_t count) {
    config.wrongCodeThreshold = count;
    addLog("CFG_THRESHOLD");
    eepromSaveConfig();
    startFeedback(FB_CONFIG_SECURITY);
}

void LockController::setAlarmTimeout(const unsigned long ms) {
    config.alarmTimeout = ms;
    addLog("CFG_ALARM");
    eepromSaveConfig();
    startFeedback(FB_CONFIG_SECURITY);
}

void LockController::setPin(const char *newPin) {
    strncpy(correctPIN, newPin, sizeof(correctPIN) - 1);
    correctPIN[sizeof(correctPIN) - 1] = '\0';
    addLog("CFG_PIN");
    eepromSaveConfig();
    startFeedback(FB_CONFIG_SECURITY);
}

void LockController::notifyKeyInput() {
    if (activeFeedback == FB_NONE || activeFeedback == FB_KEY_INPUT) {
        startFeedback(FB_KEY_INPUT);
    }
}

bool LockController::validatePIN(const char *pin, const uint8_t length) const {
    const auto correctLen = static_cast<uint8_t>(strlen(correctPIN));
    return (length == correctLen && strncmp(pin, correctPIN, correctLen) == 0);
}

uint8_t LockController::getPINLength() const {
    return static_cast<uint8_t>(strlen(correctPIN));
}


void LockController::setPinAttempts(const bool isSuccess) {
    if (currentState == STATE_ALARM) return;
    if (isSuccess) {
        wrongAttemptCount = 0;
        performUnlock("PIN");
    } else {
        wrongAttemptCount++;
        addLog("PIN_FAIL");
        if (wrongAttemptCount >= config.wrongCodeThreshold) {
            triggerAlarm();
        } else {
            startFeedback(FB_WRONG_PIN);
        }
    }
}

// --- Getters ---

LockState LockController::getState() const { return currentState; }
SystemMode LockController::getMode() const { return currentMode; }
SystemConfig LockController::getConfig() const { return config; }

void LockController::getStatusJSON(char *buffer, const size_t bufferSize) const {
    const char *stateStr = (currentState == STATE_LOCKED)
                               ? "LOCKED"
                               : (currentState == STATE_UNLOCKED)
                                     ? "UNLOCKED"
                                     : "ALARM";
    const char *modeStr = (currentMode == MODE_AUTO) ? "AUTO" : "MANUAL";
    snprintf(buffer, bufferSize,
             "{\"state\":\"%s\",\"mode\":\"%s\","
             "\"autoLockTimeout\":%lu,\"alarmTimeout\":%lu,"
             "\"wrongCodeThreshold\":%u,\"logs\":%u}",
             stateStr, modeStr,
             config.autoLockTimeout, config.alarmTimeout,
             config.wrongCodeThreshold, logCount
    );
}

LogEntry LockController::getLogEntry(const uint8_t index) const {
    if (index >= logCount) return LogEntry{0, "EMPTY"};
    const uint8_t startIndex = (logCount < LOG_SIZE) ? 0 : logHead;
    const uint8_t realIndex = (startIndex + index) % LOG_SIZE;
    return logQueue[realIndex];
}

uint8_t LockController::getLogCount() const { return logCount; }

// --- LED ---

Color LockController::getLEDColor() const {
    // Priority: Alarm > Feedback > State
    if (isAlarmActive) return {255, 0, 0};
    if (activeFeedback != FB_NONE) {
        switch (activeFeedback) {
            case FB_REMOTE_LOCK: return {255, 0, 0};
            case FB_REMOTE_UNLOCK: return {0, 255, 0};
            case FB_CONFIG: return {0, 255, 255};
            case FB_CONFIG_SECURITY: return {255, 255, 0};
            case FB_WRONG_PIN: return {255, 80, 0};
            case FB_KEY_INPUT: return {255, 255, 255};
            default: break;
        }
    }
    switch (currentState) {
        case STATE_LOCKED: return {255, 0, 0};
        case STATE_UNLOCKED: return {0, 255, 0};
        default: return {0, 0, 0};
    }
}

bool LockController::shouldBlinkLED() const {
    // Blink during alarm or active feedback events
    if (isAlarmActive) return true;
    if (activeFeedback != FB_NONE) return true;
    return false;
}

// --- EEPROM config ---

void LockController::eepromLoadConfig() {
    const uint8_t m0 = EEPROM.read(EEPROM_BASE);
    const uint8_t m1 = EEPROM.read(EEPROM_BASE + 1);

    // Validate magic bytes
    if (m0 != MAGIC_0 || m1 != MAGIC_1) {
        Serial.println("EEPROM: no config found, saving defaults");
        eepromSaveConfig();
        return;
    }

    auto *cfgBytes = reinterpret_cast<uint8_t *>(&config);
    for (size_t i = 0; i < sizeof(SystemConfig); i++) {
        cfgBytes[i] = EEPROM.read(EEPROM_CFG_ADDR + static_cast<int>(i));
    }

    for (size_t i = 0; i < sizeof(correctPIN); i++) {
        correctPIN[i] = static_cast<char>(EEPROM.read(EEPROM_PIN_ADDR + static_cast<int>(i)));
    }
    correctPIN[sizeof(correctPIN) - 1] = '\0';

    // Restore mode safely
    const uint8_t savedMode = EEPROM.read(EEPROM_MODE_ADDR);
    currentMode = (savedMode == static_cast<uint8_t>(MODE_MANUAL)) ? MODE_MANUAL : MODE_AUTO;

    Serial.print("EEPROM: config loaded");
}


void LockController::eepromSaveConfig() const {
    EEPROM.update(EEPROM_BASE, MAGIC_0);
    EEPROM.update(EEPROM_BASE + 1, MAGIC_1);

    const auto *cfgBytes = reinterpret_cast<const uint8_t *>(&config);
    for (size_t i = 0; i < sizeof(SystemConfig); i++) {
        EEPROM.update(EEPROM_CFG_ADDR + static_cast<int>(i), cfgBytes[i]);
    }

    for (size_t i = 0; i < sizeof(correctPIN); i++) {
        EEPROM.update(EEPROM_PIN_ADDR + static_cast<int>(i), static_cast<uint8_t>(correctPIN[i]));
    }

    EEPROM.update(EEPROM_MODE_ADDR, static_cast<uint8_t>(currentMode));
}

// --- Private helpers ---

void LockController::addLog(const char *event) {
    logQueue[logHead].timestamp = millis();
    strncpy(logQueue[logHead].eventType, event, sizeof(logQueue[logHead].eventType) - 1);
    logQueue[logHead].eventType[sizeof(logQueue[logHead].eventType) - 1] = '\0';
    logHead = (logHead + 1) % LOG_SIZE;
    if (logCount < LOG_SIZE) logCount++;
    Serial.println(event);
}

void LockController::triggerAlarm() {
    currentState = STATE_ALARM;
    isAlarmActive = true;
    alarmStartTime = millis();
    wrongAttemptCount = 0;
    activeFeedback = FB_NONE;
    addLog("ALARM_ON");
}

void LockController::clearAlarm() {
    isAlarmActive = false;
    currentState = STATE_LOCKED;
    addLog("ALARM_OFF");
}

void LockController::performLock(const char *source) {
    currentState = STATE_LOCKED;
    char buf[20];
    snprintf(buf, sizeof(buf), "LOCK_%s", source);
    addLog(buf);
}

void LockController::performUnlock(const char *source) {
    currentState = STATE_UNLOCKED;
    unlockStartTime = millis();
    char buf[20];
    snprintf(buf, sizeof(buf), "UNLCK_%s", source);
    addLog(buf);
}

void LockController::startFeedback(const FeedbackType type) {
    activeFeedback = type;
    feedbackStartTime = millis();
}

unsigned long LockController::feedbackDuration() const {
    return (activeFeedback == FB_KEY_INPUT)
               ? FEEDBACK_KEY_DURATION
               : FEEDBACK_DURATION;
}
