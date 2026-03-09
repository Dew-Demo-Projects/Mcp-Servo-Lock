#ifndef ASM_LOCKCONTROLLER_H
#define ASM_LOCKCONTROLLER_H

#include <Arduino.h>

struct LogEntry {
    unsigned long timestamp;
    char eventType[12];
};

struct SystemConfig {
    unsigned long autoLockTimeout;
    unsigned long alarmTimeout;
    uint8_t wrongCodeThreshold;
};

enum LockState { STATE_LOCKED, STATE_UNLOCKED, STATE_ALARM };

enum SystemMode { MODE_MANUAL, MODE_AUTO };

struct Color {
    int r, g, b;
};

enum FeedbackType {
    FB_NONE,
    FB_REMOTE_LOCK,
    FB_REMOTE_UNLOCK,
    FB_CONFIG,
    FB_CONFIG_SECURITY,
    FB_WRONG_PIN,
    FB_KEY_INPUT
};

class LockController {
public:
    LockController();

    void begin();

    void update();

    // Remote Actions
    void remoteLock();

    void remoteUnlock();

    void toggleMode();

    // Configuration
    void setAutoLockTimeout(unsigned long ms);

    void setWrongCodeThreshold(uint8_t count);

    void setAlarmTimeout(unsigned long ms);

    void setPin(const char *newPin);

    void setPinAttempts(bool isSuccess);

    [[nodiscard]] bool validatePIN(const char *pin, uint8_t length) const;

    [[nodiscard]] uint8_t getPINLength() const;


    // Input Feedback
    void notifyKeyInput();

    // State Access
    [[nodiscard]] LockState getState() const;

    [[nodiscard]] SystemMode getMode() const;

    [[nodiscard]] SystemConfig getConfig() const;

    void getStatusJSON(char *buffer, size_t bufferSize) const;

    // Logging
    [[nodiscard]] LogEntry getLogEntry(uint8_t index) const;

    [[nodiscard]] uint8_t getLogCount() const;

    // Hardware Indicators
    [[nodiscard]] Color getLEDColor() const;

    [[nodiscard]] bool shouldBlinkLED() const;

private:
    LockState currentState;
    SystemMode currentMode;
    SystemConfig config{};
    char correctPIN[9]{};

    unsigned long unlockStartTime;
    unsigned long alarmStartTime;

    uint8_t wrongAttemptCount;
    bool isAlarmActive;

    FeedbackType activeFeedback;
    unsigned long feedbackStartTime;
    static constexpr unsigned long FEEDBACK_DURATION = 3000;
    static constexpr unsigned long FEEDBACK_KEY_DURATION = 400;

    // Circular log buffer (last 30 entries)
    static constexpr uint8_t LOG_SIZE = 30;
    LogEntry logQueue[LOG_SIZE]{};
    uint8_t logHead;
    uint8_t logCount;

    // EEPROM Layout:
    // [0..1] Magic Bytes
    // [2..N] SystemConfig
    // [N+1..N+9] PIN
    // [N+10] Mode
    static constexpr int EEPROM_BASE = 0;
    static constexpr uint8_t MAGIC_0 = 0xA5;
    static constexpr uint8_t MAGIC_1 = 0x3D;
    static constexpr int EEPROM_CFG_ADDR = 2;
    static constexpr int EEPROM_PIN_ADDR = EEPROM_CFG_ADDR + (int) sizeof(SystemConfig);
    static constexpr int EEPROM_MODE_ADDR = EEPROM_PIN_ADDR + 9;

    void eepromLoadConfig();

    void eepromSaveConfig() const;

    void addLog(const char *event);

    void triggerAlarm();

    void clearAlarm();

    void performLock(const char *source);

    void performUnlock(const char *source);

    void startFeedback(FeedbackType type);

    [[nodiscard]] unsigned long feedbackDuration() const;
};

#endif
