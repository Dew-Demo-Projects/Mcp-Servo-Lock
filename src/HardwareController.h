#ifndef ASM_HARDWARECONTROLLER_H
#define ASM_HARDWARECONTROLLER_H

#include <Arduino.h>
#include "Servo.h"

// Hardware pin configuration
struct HardwareConfig {
    uint8_t ledPins[3];
    uint8_t buttonPins[6];
    uint8_t servoPin;
    uint8_t buttonCount;
    int servoLockedAngle;
    int servoUnlockedAngle;
};

class HardwareController {
public:
    HardwareController();

    void begin(const HardwareConfig &config);

    void update();

    // LED Control
    void setLEDColor(int r, int g, int b);

    void setLEDBlink(bool shouldBlink);

    void turnOffLED();

    // Button Input
    [[nodiscard]] bool isButtonPressed(uint8_t buttonIndex) const;

    bool getButtonPress(uint8_t buttonIndex); // Returns true once per press

    // Servo Control
    void setServoAngle(int angle);

    void lockMechanism();

    void unlockMechanism();

    [[nodiscard]] bool isServoMoving() const;

private:
    unsigned long systemStartTime;
    static constexpr unsigned long STARTUP_IGNORE_BUTTON_TIME = 500;

    HardwareConfig config{};
    Servo lockServo;

    // LED State
    int currentR, currentG, currentB;
    bool ledBlinkEnabled;
    unsigned long lastBlinkTime;
    bool ledOn;

    // Button State
    static constexpr unsigned long DEBOUNCE_DELAY = 50;
    unsigned long lastButtonTime[6];
    bool lastButtonState[6]; // Raw reading
    bool stableButtonState[6]; // Debounced reading
    bool buttonPressed[6]; // Event flag

    // Servo State
    int currentServoAngle;
    bool servoMoving;
    unsigned long servoMoveStartTime;
    static constexpr unsigned long SERVO_MOVE_TIME = 500;

    void updateLED();

    void updateButtons();
};

#endif
