#pragma once
#include "../core/Page.h"
#include "../core/PcLink.h"
#include <math.h>

// Dual-Mode Air Traffic Control (ATC) Flight Radar Page
// Mode 0 (Default): Full-Screen Vector Radar Scope (Clean Points Only, Zero-Flicker, Red=Mil / Green=Cargo / Yellow=Pax).
// Mode 1 (Long Press): Full-Screen Auto-Scrolling Flight Cards List (Color Coded, Red=Mil / Green=Cargo / Yellow=Pax).
class FlightRadarPage : public Page {
public:
    explicit FlightRadarPage(const PcLink &link) : _link(link) {}

    const char *name() const override { return "Flight Radar"; }

    void onEnter(Adafruit_ST7789 &tft) override {
        tft.fillScreen(ST77XX_BLACK);
        _lastDrawMs = 0;
        _sweepAngle = 0;
        _scrollOffset = 0;
        _prevCount = -1;
        _lastFlightRevision = 0;
        _cardsNeedsRedraw = true;
        _blipsNeedsRedraw = true;
        _prevItemCount = 0;
        for (int i = 0; i < 8; i++) {
            _prevBlipX[i] = _prevBlipY[i] = -1;
            _prevCat[i] = 255;
        }
        _drawStaticLayout(tft);
    }

    void onLongPress(Adafruit_ST7789 &tft) override {
        // Toggle between Mode 0 (Full Radar Scope) and Mode 1 (Flight Cards List)
        _mode = (_mode + 1) % 2;
        tft.fillScreen(ST77XX_BLACK);
        _lastDrawMs = 0;
        _scrollOffset = 0;
        _lastFlightRevision = 0;
        _cardsNeedsRedraw = true;
        _blipsNeedsRedraw = true;
        _prevItemCount = 0;
        for (int i = 0; i < 8; i++) {
            _prevBlipX[i] = _prevBlipY[i] = -1;
            _prevCat[i] = 255;
        }
        _drawStaticLayout(tft);
    }

    void update(Adafruit_ST7789 &tft) override {
        uint32_t now = millis();

        if (_mode == 0) {
            if (now - _lastDrawMs < REDRAW_INTERVAL_MS) return;
            _lastDrawMs = now;
            _sweepAngle = (_sweepAngle + 12) % 360;
            _updateRadarMode(tft);
        } else {
            _updateCardsMode(tft, now);
        }
    }

private:
    static constexpr uint32_t REDRAW_INTERVAL_MS = 150; // Smooth 6 FPS sweep animation

    // Palette
    static constexpr uint16_t COLOR_BG          = ST77XX_BLACK;
    static constexpr uint16_t COLOR_CYAN        = 0x07FF; // Bright Neon Cyan (#00E5FF)
    static constexpr uint16_t COLOR_RADAR_GRID  = 0x0250; // Dark Phosphor Green Grid
    static constexpr uint16_t COLOR_SWEEP       = 0x05E0; // Neon Green Radar Sweep
    static constexpr uint16_t COLOR_RED         = ST77XX_RED;   // Red for Military Flights (#FF0000)
    static constexpr uint16_t COLOR_GREEN       = 0x07E0;       // Green for Cargo Flights (#00FF00)
    static constexpr uint16_t COLOR_YELLOW      = 0xFFE0;       // Yellow for Passenger Flights (#FFFF00)
    static constexpr uint16_t COLOR_CARD_BG     = 0x0841;       // Dark Slate Card (#09090C)
    static constexpr uint16_t COLOR_CARD_LINE   = 0x18C3;       // Fine Border (#1C1C22)
    static constexpr uint16_t COLOR_MUTED       = 0x6CF9;       // Muted Text (#6B7280)

    const PcLink &_link;
    uint32_t _lastDrawMs = 0;
    uint32_t _lastScrollMs = 0;
    uint32_t _lastFlightRevision = 0;
    int _mode = 0; // 0 = Full Radar Scope, 1 = Flight Cards List
    int _sweepAngle = 0;
    int _scrollOffset = 0;
    int _prevCount = -1;
    bool _cardsNeedsRedraw = true;
    bool _blipsNeedsRedraw = true;

    // Previous Blip Tracker for zero-flicker selective updates (up to 8 blips)
    int _prevItemCount = 0;
    int _prevBlipX[8];
    int _prevBlipY[8];
    uint8_t _prevCat[8];

    uint16_t _getCategoryColor(uint8_t cat) const {
        if (cat == 1) return COLOR_RED;     // 🔴 Military
        if (cat == 2) return COLOR_GREEN;   // 🟢 Cargo
        return COLOR_YELLOW;               // 🟡 Passenger
    }

    void _drawStaticLayout(Adafruit_ST7789 &tft) {
        if (_mode == 0) {
            // MODE 0: Full Screen Clean Radar Scope
            int cx = 120, cy = 120;

            // Concentric Range Rings: 5km (r=24), 10km (r=48), 15km (r=72), 25km (r=104)
            tft.drawCircle(cx, cy, 24, COLOR_RADAR_GRID);
            tft.drawCircle(cx, cy, 48, COLOR_RADAR_GRID);
            tft.drawCircle(cx, cy, 72, COLOR_RADAR_GRID);
            tft.drawCircle(cx, cy, 104, COLOR_RADAR_GRID);

            // Crosshair Lines
            tft.drawFastHLine(12, cy, 216, COLOR_RADAR_GRID);
            tft.drawFastVLine(cx, 12, 216, COLOR_RADAR_GRID);
            tft.fillCircle(cx, cy, 2, COLOR_CYAN);

            // Cardinal Directions
            tft.setTextColor(COLOR_MUTED, COLOR_BG);
            tft.setTextSize(1);
            tft.setCursor(cx - 3, 14); tft.print("N");
            tft.setCursor(224, cy - 3); tft.print("E");
            tft.setCursor(cx - 3, 212); tft.print("S");
            tft.setCursor(14, cy - 3); tft.print("W");

            // Ring Distance Labels
            tft.setCursor(cx + 26, cy + 2); tft.print("5k");
            tft.setCursor(cx + 50, cy + 2); tft.print("10k");
            tft.setCursor(cx + 74, cy + 2); tft.print("15k");
            tft.setCursor(cx + 106, cy + 2); tft.print("25k");

            // Bottom Category Color Legend
            tft.fillRect(16, 228, 5, 5, COLOR_RED);
            tft.setTextColor(COLOR_MUTED, COLOR_BG);
            tft.setTextSize(1);
            tft.setCursor(25, 227); tft.print("MIL");

            tft.fillRect(84, 228, 5, 5, COLOR_GREEN);
            tft.setCursor(93, 227); tft.print("CARGO");

            tft.fillRect(164, 228, 5, 5, COLOR_YELLOW);
            tft.setCursor(173, 227); tft.print("PAX");
        } else {
            // MODE 1: Flight Cards List Header
            tft.setTextColor(COLOR_YELLOW, COLOR_BG);
            tft.setTextSize(2);
            tft.setCursor(10, 6);
            tft.print("VUELOS MADRID");

            tft.drawFastHLine(10, 26, 220, COLOR_CARD_LINE);
        }
    }

    void _updateRadarMode(Adafruit_ST7789 &tft) {
        int cx = 120, cy = 120;

        // Erase previous sweep line ONLY
        int prevRadAngle = (_sweepAngle - 12 + 360) % 360;
        float prevRad = (prevRadAngle - 90) * 0.0174533f;
        int px = cx + (int)(102 * cos(prevRad));
        int py = cy + (int)(102 * sin(prevRad));
        tft.drawLine(cx, cy, px, py, COLOR_BG);

        // Re-draw grid lines under sweep line
        tft.drawCircle(cx, cy, 24, COLOR_RADAR_GRID);
        tft.drawCircle(cx, cy, 48, COLOR_RADAR_GRID);
        tft.drawCircle(cx, cy, 72, COLOR_RADAR_GRID);
        tft.drawCircle(cx, cy, 104, COLOR_RADAR_GRID);
        tft.drawFastHLine(12, cy, 216, COLOR_RADAR_GRID);
        tft.drawFastVLine(cx, 12, 216, COLOR_RADAR_GRID);
        tft.fillCircle(cx, cy, 2, COLOR_CYAN);

        // Draw current sweep line
        float curRad = (_sweepAngle - 90) * 0.0174533f;
        int sx = cx + (int)(102 * cos(curRad));
        int sy = cy + (int)(102 * sin(curRad));
        tft.drawLine(cx, cy, sx, sy, COLOR_SWEEP);

        // Check if new real-time flight data arrived from PC serial link
        uint32_t currentRev = _link.flightRevision();
        if (currentRev != _lastFlightRevision) {
            _lastFlightRevision = currentRev;
            _blipsNeedsRedraw = true;
        }

        // Flight Count Badge (Top Right)
        int fCount = _link.flightCount();
        if (fCount != _prevCount) {
            _prevCount = fCount;
            tft.fillRect(145, 6, 90, 18, COLOR_BG);
            tft.setTextColor(COLOR_MUTED, COLOR_BG);
            tft.setTextSize(1);
            tft.setCursor(148, 10);
            char cntBuf[16];
            snprintf(cntBuf, sizeof(cntBuf), "[%d VUELOS]", fCount);
            tft.print(cntBuf);
        }

        // Selective-update blips: Only erase and redraw blips that actually changed coordinates (Zero-Flicker!)
        if (_blipsNeedsRedraw) {
            _blipsNeedsRedraw = false;

            int itemCount = _link.flightItemCount();
            if (itemCount > 8) itemCount = 8;
            if (itemCount == 0 && _link.flightActive()) itemCount = 1;

            // Erase surplus blips from previous frame
            for (int i = itemCount; i < _prevItemCount; i++) {
                if (_prevBlipX[i] >= 0) {
                    tft.fillRect(_prevBlipX[i] - 3, _prevBlipY[i] - 3, 7, 7, COLOR_BG);
                    _prevBlipX[i] = -1;
                }
                _prevCat[i] = 255;
            }
            _prevItemCount = itemCount;

            for (int i = 0; i < itemCount; i++) {
                PcLink::FlightItem item = _link.getFlightItem(i);
                if (item.callsign.isEmpty()) {
                    if (_prevBlipX[i] >= 0) {
                        tft.fillRect(_prevBlipX[i] - 3, _prevBlipY[i] - 3, 7, 7, COLOR_BG);
                    }
                    _prevBlipX[i] = -1;
                    _prevCat[i] = 255;
                    continue;
                }

                float dist = item.dist;
                uint16_t brg = item.heading;

                // Map distance 0..25km to 0..102px
                float mapRadius = (dist / 25.0f) * 102.0f;
                if (mapRadius > 102.0f) mapRadius = 102.0f;
                if (mapRadius < 6.0f) mapRadius = 6.0f;

                float planeRad = (brg - 90) * 0.0174533f;
                int bx = cx + (int)(mapRadius * cos(planeRad));
                int by = cy + (int)(mapRadius * sin(planeRad));
                uint8_t cat = item.category;

                // Check if this blip position or category changed
                bool posChanged = (bx != _prevBlipX[i] || by != _prevBlipY[i] || cat != _prevCat[i]);

                if (posChanged) {
                    // Erase old blip position
                    if (_prevBlipX[i] >= 0) {
                        tft.fillRect(_prevBlipX[i] - 3, _prevBlipY[i] - 3, 7, 7, COLOR_BG);
                    }

                    // Get category color: Red=Mil, Green=Cargo, Yellow=Pax
                    uint16_t blipColor = _getCategoryColor(cat);

                    // Draw clean solid 5x5 point blip (NO text tags!)
                    tft.fillRect(bx - 2, by - 2, 5, 5, blipColor);

                    // Save state
                    _prevBlipX[i] = bx;
                    _prevBlipY[i] = by;
                    _prevCat[i] = cat;
                }
            }
        }

        // Keep all active blips rendered on top of the radar sweep line so the sweep never erases them
        for (int i = 0; i < _prevItemCount; i++) {
            if (_prevBlipX[i] >= 0 && _prevCat[i] != 255) {
                tft.fillRect(_prevBlipX[i] - 2, _prevBlipY[i] - 2, 5, 5, _getCategoryColor(_prevCat[i]));
            }
        }
    }

    void _updateCardsMode(Adafruit_ST7789 &tft, uint32_t now) {
        int itemCount = _link.flightItemCount();
        if (itemCount > 8) itemCount = 8;
        if (itemCount == 0 && _link.flightActive()) itemCount = 1;

        // Check if new real-time flight data arrived from PC serial link
        uint32_t currentRev = _link.flightRevision();
        if (currentRev != _lastFlightRevision) {
            _lastFlightRevision = currentRev;
            _cardsNeedsRedraw = true;
        }

        // Auto-scroll list offset every 3.0 seconds if more than 2 flights
        if (itemCount > 2 && (now - _lastScrollMs > 3000)) {
            _lastScrollMs = now;
            _scrollOffset = (_scrollOffset + 1) % itemCount;
            _cardsNeedsRedraw = true;
        }

        if (!_cardsNeedsRedraw) return;
        _cardsNeedsRedraw = false;

        tft.fillRect(0, 30, 240, 210, COLOR_BG);

        if (itemCount == 0 || !_link.flightActive()) {
            tft.setTextColor(COLOR_MUTED, COLOR_BG);
            tft.setTextSize(2);
            tft.setCursor(12, 80);
            tft.print("SIN VUELOS CERCA");
            tft.setTextSize(1);
            tft.setCursor(12, 110);
            tft.print("Escaneando cielo de Madrid...");
            return;
        }

        int startY = 32;
        int cardHeight = 100;

        for (int i = 0; i < 2; i++) {
            int flightIdx = (_scrollOffset + i) % itemCount;
            PcLink::FlightItem item = _link.getFlightItem(flightIdx);

            int cardY = startY + i * (cardHeight + 4);
            uint16_t catColor = _getCategoryColor(item.category);
            uint16_t borderColor = (item.category != 0) ? catColor : COLOR_CARD_LINE;

            tft.fillRoundRect(6, cardY, 228, cardHeight, 6, COLOR_CARD_BG);
            tft.drawRoundRect(6, cardY, 228, cardHeight, 6, borderColor);

            // Row 1: Callsign & Category/Airline Tag
            tft.setTextColor(catColor, COLOR_CARD_BG);
            tft.setTextSize(2);
            tft.setCursor(12, cardY + 8);
            tft.print(item.callsign);

            tft.setTextSize(2);
            tft.setCursor(120, cardY + 8);
            if (item.category == 1) {
                tft.setTextColor(COLOR_RED, COLOR_CARD_BG);
                tft.print("MILITAR");
            } else if (item.category == 2) {
                tft.setTextColor(COLOR_GREEN, COLOR_CARD_BG);
                tft.print("CARGO");
            } else {
                tft.setTextColor(COLOR_CYAN, COLOR_CARD_BG);
                tft.print(item.airline);
            }

            // Row 2: Large Distance & Cardinal Direction
            tft.setTextColor(COLOR_YELLOW, COLOR_CARD_BG);
            tft.setTextSize(2);
            tft.setCursor(12, cardY + 36);
            char r2[32];
            snprintf(r2, sizeof(r2), "%.1f km  (%s)", item.dist, item.dir.c_str());
            tft.print(r2);

            // Row 3: Large Altitude & Speed
            tft.setTextColor(ST77XX_WHITE, COLOR_CARD_BG);
            tft.setTextSize(2);
            tft.setCursor(12, cardY + 66);
            char r3[32];
            snprintf(r3, sizeof(r3), "%.1fkm | %dkmh", item.alt / 1000.0f, item.spd);
            tft.print(r3);
        }
    }
};
