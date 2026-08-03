# ESP32-C3 Desktop Companion Monitor

[![Framework](https://img.shields.io/badge/Framework-Arduino_ESP32-blue.svg)](https://espressif.com)
[![Hardware](https://img.shields.io/badge/Hardware-ESP32--C3_Supermini-orange.svg)](https://espressif.com)
[![Enclosure](https://img.shields.io/badge/Enclosure-3D_Printed_Bambulab_P2S-purple.svg)](https://bambulab.com)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

An interactive, zero-flicker desktop companion monitor powered by an **ESP32-C3 Supermini**, an **ST7789 240x240 SPI display**, a **TTP223 capacitive touch sensor**, and a **3D-printed enclosure crafted on a Bambulab P2S**.

Includes 5 customizable pages, 5 watch faces (including a 3D Word Clock replica), dual AI assistant monitoring (Claude Code & Antigravity AI), Open-Meteo weather integration, Matrix digital rain, Star Wars opening crawl credits, Pac-Man arcade action, and an AFK sleeping mascot mode.

---

## 🌟 Key Features

1. **Clock (5 Customizable Watch Faces)**:
   * **Face 0 (3D WordClock "Hablando Claro" - Default)**: Dynamic Spanish conversational time phrases with 5-minute zero-flicker buckets, time-of-day contextual humor (lunch, leaving work, morning coffee, sleep time), multi-color LED neon words, and centered weather complication.
   * **Face 1 (Minimal Digital)**: Clean digital layout with Wi-Fi status and weather complication.
   * **Face 2 (Apple/Nike Duo-Color GIANT)**: Stacked size 10 digits with electric blue & coral theme.
   * **Face 3 (Classic Analog Chronograph)**: Zero-flicker 1-pixel differential hand erasing with sub-dials.
   * **Face 4 (Retro Striped)**: Cream & gold stacked numbers with `DD-MM-YYYY` date formatting.
   * **Vector Weather Icons**: Sun ☀️ / Moon 🌙 (day/night API detection), Cloud ☁️, Rain 🌧️, and Storm 🌩️ drawn with GFX primitives.
2. **Dedicated Weather Page**:
   * Powered by **Open-Meteo API** (100% Free, **Zero API key required**).
   * Displays temperature, a large **32x32px vector weather icon**, condition text, relative humidity, and centered API credit.
3. **Dual AI Assistant Monitor (Claude Code & Antigravity AI)**:
   * Long-press toggle between **Claude Code** and **Antigravity AI**.
   * **Claude Code Mode**: Session & weekly token usage progress bars in Anthropic Obsidian dark mode.
   * **Antigravity AI Mode**: Active model header badge (`Gemini 3.6 Pro`), Card 1: `SUBAGENTES` count & progress bar, Card 2: `CUOTA AGY` context window percentage + active tool label (`Tool: run_command`, `replace_file`, etc.).
   * **Offline Zero State**: Automatically zeroed values (`0 K` tokens, `0%` progress bars, `--` badges) when USB CDC link is offline.
4. **Claude AI Companion Mascot**:
   * Animated pixel-art robot mascot with speech bubble (*"Hola Alejo!"*), facial expressions, and touch-petted heart reaction (`<3 PURR! <3`).
5. **Matrix Rain, Star Wars Credits & Arcade Page**:
   * **Mode 0 (Matrix Rain)**: 60fps green digital rain screen refresher.
   * **Mode 1 (Star Wars Opening Crawl)**: Custom Star Wars intro sequence with twinkling starfield, yellow credits crawl, and project backstory.
   * **Mode 2 (Pac-Man Arcade)**: Classic Pac-Man animation with ghost color cycling (Blinky 🔴, Pinky 💖, Inky 💙, Clyde 🧡).
   * **Mode 3 (AFK Sleep Mode)**: Cozy 3D-styled pixel art bedroom with Claude mascot sleeping in a wooden bed with floating `Zzz` animation.
6. **Wi-Fi Captive Portal & Smart NVS Auto-Sync**:
   * Hotspot AP `Companion-Setup` on `http://192.168.4.1` with NVS Flash memory storage. Automatically syncs and updates saved Wi-Fi if `src/config.h` is edited.

---

## 🔌 Wiring Diagram

For detailed wire color mappings and mounting instructions, see **[wiring_diagram.md](wiring_diagram.md)**.

| ST7789 Pin | ESP32-C3 GPIO |
|------------|---------------|
| SCL / SCK   | GPIO 4        |
| SDA / MOSI  | GPIO 6        |
| CS          | GPIO 7        |
| DC          | GPIO 2        |
| RST         | GPIO 10       |
| BLK         | GPIO 3        |
| VCC         | 3V3           |
| GND         | GND           |

| TTP223 Touch Pin | ESP32-C3 GPIO |
|------------------|---------------|
| OUT              | GPIO 5        |
| VCC              | 3V3           |
| GND              | GND           |

---

## 🛠️ Build & Flash Instructions

1. Clone the repository:
   ```bash
   git clone https://github.com/Alejandrokbo/ESP32-C3-desktop-companion.git
   cd ESP32-C3-desktop-companion
   ```
2. Create your local secrets file:
   ```bash
   cp src/config.h.example src/config.h
   ```
3. Edit `src/config.h` with your Wi-Fi credentials or latitude/longitude coordinates (or leave empty to configure via Wi-Fi AP setup portal).
4. Build and upload using [PlatformIO](https://platformio.org/):
   ```bash
   pio run -t upload
   ```

---

## 🐍 PC Agent Script

The PC side script streams live Claude Code & Antigravity AI usage metrics to the ESP32 over USB CDC Serial (with auto-reconnect on port disconnect):

```bash
cd pc-agent
pip install -r requirements.txt
python companion_agent.py /dev/cu.usbmodem1101
```

---

## 📄 License

Distributed under the MIT License. See `LICENSE` for more information.
