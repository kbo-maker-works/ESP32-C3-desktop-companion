# CLAUDE.md — ESP32-C3 Desktop Companion Monitor

## 📌 Project Overview
Desktop companion device providing live Claude Code AI usage metrics, weather forecast, clock watchfaces, pixel-art mascot, and arcade animations.

- **Hardware**: ESP32-C3 Supermini, ST7789 240x240 SPI Display, TTP223 Capacitive Touch Sensor.
- **Enclosure**: Custom 3D-printed case designed for Bambu Lab P2S.
- **Framework**: PlatformIO / Arduino framework with Adafruit GFX + ST7789.

---

## 🔌 Hardware & Wiring Reference

| Component | Signal | ESP32-C3 GPIO | Wire Color |
|---|---|---|---|
| ST7789 Display | SCL / SCK | GPIO 4 | 🟡 Yellow |
| ST7789 Display | SDA / MOSI | GPIO 6 | 🟢 Green |
| ST7789 Display | CS | GPIO 7 | 🟣 Purple |
| ST7789 Display | DC / RS | GPIO 2 | 🔵 Blue |
| ST7789 Display | RES / RST | GPIO 10 | ⚪ White |
| ST7789 Display | BLK / LED | GPIO 3 | 🟧 Orange |
| ST7789 Display | VCC / GND | 3V3 / GND | 🔴 Red / ⬛ Black |
| TTP223 Touch | OUT / SIG | GPIO 5 | 🟤 Brown |
| TTP223 Touch | VCC / GND | 3V3 / GND | 🔴 Red / ⬛ Black |

---

## 🚀 Build & Development Commands

- **Build Firmware**: `pio run`
- **Flash Firmware**: `pio run -t upload`
- **Serial Monitor**: `pio device monitor`
- **Run PC Agent**:
  ```bash
  cd pc-agent
  pip install -r requirements.txt
  python companion_agent.py <SERIAL_PORT>   # e.g., COM5 or /dev/tty.usbmodem...
  ```

---

## 🧩 Architecture & Features

1. **Pages (`src/pages/`)**:
   - `ClockPage.h`: 4 watch faces (Minimal, Nike Duo-Color GIANT, Classic Analog Chronograph, Retro Striped) + Open-Meteo vector weather icons. Zero-flicker 1-pixel differential rendering.
   - `WeatherPage.h`: Open-Meteo API integration (Free, no API key). Shows temp, humidity, vector icons (Sun, Moon, Cloud, Rain, Storm).
   - `ClaudeUsagePage.h`: Unified AI Assistant Monitor (Pure Pitch-Black OLED background `#000000`). **Long Press** on touch button toggles between **CLAUDE CODE** (Terracotta `#D97757` & Lavender `#A4B2E6`) and **ANTIGRAVITY AI** (Muted Steel Cyan `#0EA5E9` & DeepMind Emerald `#14B8A6`). Zero-glare contrast with Anthropic & Antigravity vector logos.
   - `ClawMascotPage.h`: Official Claude Code retro-robot mascot ("Hola Alejo!", `<3 PURR! <3`), mood expressions, & touch-petted reaction.
   - `MatrixRefresherPage.h`: OLED anti-burn-in animation & matrix code rain effect.
   - `SystemMonitorPage.h`: System health metrics.

2. **Core System (`src/core/`)**:
   - `DisplayManager`: GFX canvas & ST7789 display controller.
   - `TouchButton`: TTP223 capacitive touch interrupt & gesture handler.
   - `PageManager`: Touch-triggered smooth page transitions.
   - `PcLink`: Parses JSON over Serial CDC from Python PC Agent (2048-byte RX buffer).
   - `NetManager`: Wi-Fi AP captive portal setup (`Companion-Setup` on `http://192.168.4.1`) with interactive network scanner, show/hide password toggle (`👁️` / `🙈`), & NTP sync.

3. **PC Agent (`pc-agent/`)**:
   - `companion_agent.py`: Background agent streaming Claude Code usage & tokens + real-time Antigravity AI metrics (model, tool, context window, subagents) to ESP32 via USB Serial. Clean, zero-overhead execution.
