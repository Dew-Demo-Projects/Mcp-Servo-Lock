#include "HardwareController.h"

HardwareController::HardwareController()
    : systemStartTime(0), currentR(0), currentG(0), currentB(0),
      ledBlinkEnabled(false), lastBlinkTime(0), ledOn(true),
      lastButtonTime{0},
      lastButtonState{false},
      stableButtonState{false},
      buttonPressed{false},
      currentServoAngle(-1), servoMoving(false), servoMoveStartTime(0) {
}

void HardwareController::begin(const HardwareConfig &cfg) {
    config = cfg;
    systemStartTime = millis();

    // Initialize LED pins
    for (const uint8_t pin: config.ledPins) {
        pinMode(pin, OUTPUT);
        analogWrite(pin, 0);
    }

    // Initialize button pins with pull-ups
    for (uint8_t i = 0; i < config.buttonCount; i++) {
        pinMode(config.buttonPins[i], INPUT_PULLUP);
        lastButtonState[i] = true;
        stableButtonState[i] = true;
        lastButtonTime[i] = 0;
        buttonPressed[i] = false;
    }

    // Initialize servo to a locked position
    lockServo.attach(config.servoPin);
    lockServo.write(config.servoLockedAngle);
    currentServoAngle = config.servoLockedAngle;
    servoMoving = false;
}

void HardwareController::update() {
    updateLED();
    updateButtons();
    // Track servo movement duration non-blocking
    if (servoMoving && (millis() - servoMoveStartTime >= SERVO_MOVE_TIME)) {
        servoMoving = false;
    }
}

// --- LED ---

void HardwareController::setLEDColor(const int r, const int g, const int b) {
    currentR = r;
    currentG = g;
    currentB = b;
}

void HardwareController::setLEDBlink(const bool shouldBlink) {
    if (shouldBlink && !ledBlinkEnabled) {
        // Force ON state immediately to ensure visibility during short feedback windows
        ledOn = true;
        lastBlinkTime = millis();
        analogWrite(config.ledPins[0], currentR);
        analogWrite(config.ledPins[1], currentG);
        analogWrite(config.ledPins[2], currentB);
    }
    ledBlinkEnabled = shouldBlink;
    if (!shouldBlink) {
        ledOn = true; // Reset state for next activation
    }
}

void HardwareController::turnOffLED() {
    currentR = currentG = currentB = 0;
    analogWrite(config.ledPins[0], 0);
    analogWrite(config.ledPins[1], 0);
    analogWrite(config.ledPins[2], 0);
}

void HardwareController::updateLED() {
    if (!ledBlinkEnabled) {
        // Static color mode
        analogWrite(config.ledPins[0], currentR);
        analogWrite(config.ledPins[1], currentG);
        analogWrite(config.ledPins[2], currentB);
    } else {
        // Blink mode: toggle state every 500ms
        if (const unsigned long now = millis(); now - lastBlinkTime >= 500) {
            lastBlinkTime = now;
            ledOn = !ledOn;
            analogWrite(config.ledPins[0], ledOn ? currentR : 0);
            analogWrite(config.ledPins[1], ledOn ? currentG : 0);
            analogWrite(config.ledPins[2], ledOn ? currentB : 0);
        }
    }
}

// --- Buttons ---

bool HardwareController::isButtonPressed(const uint8_t buttonIndex) const {
    if (buttonIndex >= config.buttonCount) return false;
    return (digitalRead(config.buttonPins[buttonIndex]) == LOW);
}

bool HardwareController::getButtonPress(uint8_t buttonIndex) {
    if (buttonIndex >= config.buttonCount) return false;
    const bool wasPressed = buttonPressed[buttonIndex];
    buttonPressed[buttonIndex] = false; // Consume event
    return wasPressed;
}

void HardwareController::updateButtons() {
    const unsigned long now = millis();

    // Ignore inputs during startup to prevent false triggers
    if (now - systemStartTime < STARTUP_IGNORE_BUTTON_TIME) {
        for (uint8_t i = 0; i < config.buttonCount; i++) {
            lastButtonState[i] = (digitalRead(config.buttonPins[i]) == LOW);
            stableButtonState[i] = lastButtonState[i];
            lastButtonTime[i] = now;
            buttonPressed[i] = false;
        }
        return;
    }

    for (uint8_t i = 0; i < config.buttonCount; i++) {
        // Active-low: LOW pin = button pressed
        const bool reading = (digitalRead(config.buttonPins[i]) == LOW);

        // Track raw state changes
        if (reading != lastButtonState[i]) {
            lastButtonState[i] = reading;
            lastButtonTime[i] = now;
        }

        // Confirm a stable state after debounced delay
        if ((now - lastButtonTime[i]) >= DEBOUNCE_DELAY) {
            if (reading && !stableButtonState[i]) {
                buttonPressed[i] = true;
            }
            stableButtonState[i] = reading;
        }
    }
}

// --- Servo ---

void HardwareController::setServoAngle(const int angle) {
    if (angle != currentServoAngle) {
        lockServo.write(angle);
        currentServoAngle = angle;
        servoMoveStartTime = millis();
        servoMoving = true;
    }
}

void HardwareController::lockMechanism() { setServoAngle(config.servoLockedAngle); }
void HardwareController::unlockMechanism() { setServoAngle(config.servoUnlockedAngle); }
bool HardwareController::isServoMoving() const { return servoMoving; }
