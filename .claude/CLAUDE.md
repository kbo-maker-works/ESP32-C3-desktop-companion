# Memory context for Claude & Antigravity AI Assistants

Repository: `ESP32-C3-desktop-companion`
Location: `/Users/alejandro.cabo/Things/ESP32-C3-desktop-companion`

## Overview
This repository contains the complete firmware (PlatformIO / C++) and PC bridge script (Python) for the **ESP32-C3 Desktop Companion Monitor**.

### Hardware Specs
- **MCU**: ESP32-C3 Supermini (RISC-V, USB-C, 22x18mm)
- **Display**: ST7789 1.54" IPS LCD (240x240 px, SPI)
- **Input**: TTP223 Capacitive Touch Sensor (GPIO 5)
- **Enclosure**: Bambu Lab P2S 3D printed case (25°-35° desk angle)

### Main Software Modules
1. **Clock & Watch Faces**: Minimal Digital, Nike Duo-Color GIANT, Classic Analog (zero-flicker 1-px hand erase), Retro Striped.
2. **Weather Integration**: Open-Meteo REST API with dynamic day/night vector graphics (Sun, Moon, Cloud, Rain, Storm).
3. **Claude Code Monitor**: Session and weekly token quota UI in Anthropic obsidian palette.
4. **Interactive Mascot**: Animated robot mascot ("Hola Alejo!"), responds to TTP223 touch with heart/purr animation.
5. **Animations & Arcade**: Matrix digital rain (60fps), Star Wars crawl, Pac-Man ghost animations, AFK bed sleep state (`Zzz`).
6. **Captive Portal**: `Companion-Setup` on `192.168.4.1` for Wi-Fi provisioning.
7. **PC Agent (`pc-agent/companion_agent.py`)**: Streams metrics over USB CDC Serial.

### Cable & Pin Connections
- **ST7789**: SCK=GPIO4 (Yellow), MOSI=GPIO6 (Green), CS=GPIO7 (Purple), DC=GPIO2 (Blue), RST=GPIO10 (White), BLK=GPIO3 (Orange), VCC=3V3 (Red), GND=GND (Black).
- **TTP223 Touch**: OUT=GPIO5 (Brown), VCC=3V3 (Red), GND=GND (Black).
