#include <Arduino.h>
#include <WiFiS3.h>
#include "LockController.h"
#include "HardwareController.h"
#include "HTTPServer.h"

const char WIFI_SSID[] = "wifi_ssid";
const char WIFI_PASS[] = "wifi_password";

LockController lockController;
HardwareController hardwareController;
HTTPServer httpServer(lockController, 80);

HardwareConfig hwConfig = {
    .ledPins = {6, 5, 3},
    .buttonPins = {12, 11, 10, 9, 8, 7},
    .servoPin = 2,
    .buttonCount = 6,
    .servoLockedAngle = 0,
    .servoUnlockedAngle = 180
};

constexpr uint8_t MAX_PIN_LENGTH = 8;
char enteredPIN[MAX_PIN_LENGTH + 1];
uint8_t pinIndex = 0;
bool pinEntryActive = true;

// Simple single-shot timer struct
struct Timer {
    unsigned long triggerTime;

    void (*callback)();

    bool active;
};

Timer pendingTimer = {0, nullptr, false};

void setTimeout(void (*callback)(), unsigned long delayMs) {
    pendingTimer = {millis() + delayMs, callback, true};
}

void checkTimers() {
    if (pendingTimer.active && millis() >= pendingTimer.triggerTime) {
        if (pendingTimer.callback) pendingTimer.callback();
        pendingTimer.active = false;
    }
}

void processButtonPress(const uint8_t buttonIndex) {
    if (!pinEntryActive) return;

    const uint8_t pinLength = lockController.getPINLength();
    if (pinIndex >= pinLength) return;

    enteredPIN[pinIndex++] = static_cast<char>('0' + buttonIndex);
    lockController.notifyKeyInput();

    if (pinIndex >= pinLength) {
        pinEntryActive = false;
        enteredPIN[pinLength] = '\0';
        const bool isValid = lockController.validatePIN(enteredPIN, pinLength);
        lockController.setPinAttempts(isValid);

        pinIndex = 0;
        memset(enteredPIN, 0, sizeof(enteredPIN));
        setTimeout([]() { pinEntryActive = true; }, 1000);
    }
}

// --- WiFi Setup ---
void connectWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.print("Waiting for wifi");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        delay(1000);
    }

    Serial.println("\nWiFi connected!");

    auto arduinoIP = WiFi.localIP();
    const auto defaultIP = IPAddress(0, 0, 0, 0);

    Serial.print("Waiting for dhcp");
    while (arduinoIP == defaultIP) {
        Serial.print('.');
        arduinoIP = WiFi.localIP();
        delay(1000);
    }

    Serial.println("\nAddress acquired!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void setup() {
    Serial.begin(9600);
    while (!Serial) { delay(50); }

    lockController.begin();
    hardwareController.begin(hwConfig);

    // Initialize LED state
    Color c = lockController.getLEDColor();
    hardwareController.setLEDColor(c.r, c.g, c.b);
    hardwareController.setLEDBlink(lockController.shouldBlinkLED());

    connectWiFi();
    httpServer.begin();

    Serial.println("System ready.");
    char statusBuf[200];
    lockController.getStatusJSON(statusBuf, sizeof(statusBuf));
    Serial.println(statusBuf);
}

void loop() {
    // 1. Update hardware drivers
    hardwareController.update();

    // 2. Update logic state machine
    lockController.update();

    // 3. Process timers
    checkTimers();

    // 4. Handle button inputs for PIN entry
    for (uint8_t i = 0; i < hwConfig.buttonCount; i++) {
        if (hardwareController.getButtonPress(i)) {
            processButtonPress(i);
            Serial.print("Button ");
            Serial.println(i);
        }
    }

    // 5. Sync LED indicators
    Color led = lockController.getLEDColor();
    hardwareController.setLEDColor(led.r, led.g, led.b);
    hardwareController.setLEDBlink(lockController.shouldBlinkLED());

    // 6. Sync servo position
    static LockState lastServoState = STATE_LOCKED;
    const LockState curState = lockController.getState();
    if (curState != lastServoState) {
        if (curState == STATE_UNLOCKED) hardwareController.unlockMechanism();
        else hardwareController.lockMechanism();
        lastServoState = curState;
    }

    // 7. Handle HTTP clients
    httpServer.handleClient();

    // 8. Periodic status log
    static unsigned long lastStatusTime = 0;
    if (millis() - lastStatusTime >= 5000) {
        lastStatusTime = millis();
        char buf[200];
        lockController.getStatusJSON(buf, sizeof(buf));
        Serial.println(buf);
    }

    delay(10);
}
