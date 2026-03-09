# Smart Lock Controller

An Arduino UNO R4 WiFi-based smart lock system with physical PIN entry, RGB status LED, servo-actuated, HTTP
remote control, and an MCP server for AI agent integration.

---

## Hardware

| Component           | Role                  | Pins                   |
|---------------------|-----------------------|------------------------|
| RGB LED             | Status indicator      | D6 (R), D5 (G), D3 (B) |
| 6x Push Buttons     | PIN digit input (0–5) | D12–D7                 |
| Servo Motor         | Lock/unlock mechanism | D2                     |
| Arduino UNO R4 WiFi | MCU + WiFi            | —                      |
| PC                  | MCP                   | —                      |

- **Locked angle:** 0°
- **Unlocked angle:** 180°

---

## Architecture

The firmware is split into three cooperating modules:

- **`HardwareController`** — low-level LED, button debouncing, and servo control
- **`LockController`** — state machine (LOCKED / UNLOCKED / ALARM), PIN validation, feedback, EEPROM persistence, and
  event logging
- **`HTTPServer`** — lightweight GET-only HTTP API running on port 80
- **`mcpserver/`** — Python FastMCP server that wraps the HTTP API as AI-callable tools

---

## Physical PIN Entry

Six buttons map to digits **0–5**. The PIN is always **between 1-8 digits** long.

1. Press buttons in sequence to enter PIN.
2. On a correct PIN, the lock opens.
3. After `wrongCodeThreshold` consecutive failures (default: 3), the alarm activates.
4. The alarm auto-clears after `alarmTimeout` ms (default: 10 s).

---

## LED Color Reference

| Color                                  | Meaning                                           |
|----------------------------------------|---------------------------------------------------|
| $${\color{red}⬤}$$  Solid Red          | Locked                                            |
| $${\color{green}⬤}$$  Solid Green      | Unlocked                                          |
| $${\color{red}⬤}$$  Blinking Red       | Alarm active                                      |
| $${\color{green}⬤}$$  Blinking Green   | Remote unlock feedback                            |
| $${\color{red}⬤}$$  Blinking Red       | Remote lock feedback                              |
| $${\color{cyan}⬤}$$  Blinking Cyan     | Config changed                                    |
| $${\color{yellow}⬤}$$ Blinking Yellow  | Security config changed (PIN / threshold / alarm) |
| $${\color{orange}⬤}$$  Blinking Orange | Wrong PIN entered                                 |
| $${\color{white}⬤}$$  Blinking White   | Key input registered                              |

---

## Operating Modes

| Mode       | Behavior                                                                |
|------------|-------------------------------------------------------------------------|
| **AUTO**   | Lock re-engages automatically after `autoLockTimeout` ms (default: 5 s) |
| **MANUAL** | Lock only changes state via explicit commands                           |

Mode persists across reboots via EEPROM.

---

## HTTP API

All endpoints use **GET**. The device IP is printed over Serial on boot.

| Endpoint                      | Description                                |
|-------------------------------|--------------------------------------------|
| `GET /status`                 | Returns state, mode, config, and log count |
| `GET /lock`                   | Remote lock                                |
| `GET /unlock`                 | Remote unlock                              |
| `GET /toggle_mode`            | Toggle AUTO ↔ MANUAL                       |
| `GET /set_timeout?ms=N`       | Set auto-lock timeout (ms)                 |
| `GET /set_threshold?count=N`  | Set wrong-PIN threshold (1–10)             |
| `GET /set_alarm_timeout?ms=N` | Set alarm duration (ms)                    |
| `GET /set_pin?pin=XXXX`       | Change PIN (1–8 digits)                    |
| `GET /logs`                   | Return all log entries                     |
| `GET /logs?n=N`               | Return last N log entries                  |

### Example

```
GET /status
→ {"state":"LOCKED","mode":"AUTO","autoLockTimeout":5000,"alarmTimeout":10000,"wrongCodeThreshold":3,"logs":4}
```

---

## EEPROM Layout

Configuration and PIN survive power cycles:

```
[0–1]    Magic bytes (0xA5, 0x3D)
[2–10]   SystemConfig (autoLockTimeout, alarmTimeout, wrongCodeThreshold)
[11–19]  PIN string (up to 8 chars + null)
[20]     Operating mode
```

Defaults are written on first boot (magic bytes missing).

---

## Event Log

A circular buffer stores the last **30** timestamped events in RAM (not persisted across reboots).

Common event types: `SYSTEM_INIT`, `LOCK_PIN`, `LOCK_AUTO`, `LOCK_REMOTE`, `UNLCK_PIN`, `UNLCK_REMOTE`, `PIN_FAIL`,
`ALARM_ON`, `ALARM_OFF`, `MODE_AUTO`, `MODE_MANUAL`, `CFG_TIMEOUT`, `CFG_THRESHOLD`, `CFG_ALARM`, `CFG_PIN`.

---

## MCP Server (AI Integration)

The `mcpserver/` directory contains a **FastMCP** server that exposes all HTTP actions as structured tools consumable by
AI agents (e.g. Claude via MCP protocol).

### Setup

```bash
cd mcpserver
pip install fastmcp httpx
python main.py
```

Runs on `0.0.0.0:8088` via SSE transport. Update `DEVICE_BASE_URL` in `main.py` to match your device's IP.

---

## Build & Flash

Requires [PlatformIO](https://platformio.org/).

```bash
# Build
pio run

# Upload
pio run --target upload

# Monitor serial
pio device monitor --baud 9600
```

Dependencies (auto-installed by PlatformIO): `Servo`, `EEPROM`.

---

## Configuration

Edit `main.cpp` before flashing:

```cpp
const char WIFI_SSID[] = "your_ssid";
const char WIFI_PASS[] = "your_password";
```

Default PIN is `1234` (changeable via `/set_pin` after boot).
