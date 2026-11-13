#pragma once
#include "../core/Page.h"
#include "../core/PcLink.h"

// Interactive Mascot page for the official Claude Code retro-robot mascot!
// Rock-solid differential rendering with ZERO flicker.
class ClawMascotPage : public Page {
public:
    explicit ClawMascotPage(PcLink &link) : _link(link) {}

    const char *name() const override { return "Claude Mascot"; }

    void onEnter(Adafruit_ST7789 &tft) override {
        tft.fillScreen(ST77XX_BLACK);
        
        _lastDrawMs = 0;
        _petTimerMs = 0;
        _lastMood = "";
        
        // Draw initial static elements
        _drawStaticMascot(tft);
    }

    // True while a Claude Code hook alert is on screen (not yet expired/dismissed).
    bool alertActive() const {
        uint32_t seq = _link.clawEventSeq();
        return seq != 0 && seq != _dismissedEventSeq &&
               (millis() - _link.clawEventRxMs() < EVENT_SHOW_MS);
    }

    void dismissAlert() { _dismissedEventSeq = _link.clawEventSeq(); }

    void onLongPress(Adafruit_ST7789 &tft) override {
        _dismissedEventSeq = _link.clawEventSeq(); // Petting also acknowledges a pending alert
        _petTimerMs = millis(); // Trigger "PETTED" reaction for 3 seconds!
        _lastDrawMs = 0;
        _lastMood = ""; // Force mood redraw
    }

    void update(Adafruit_ST7789 &tft) override {
        uint32_t now = millis();
        if (now - _lastDrawMs < REDRAW_INTERVAL_MS) return;
        _lastDrawMs = now;

        _animFrame = (_animFrame + 1) % 2;

        // Determine current mood
        String mood = _link.clawMood();
        bool isPetted = (now - _petTimerMs < 3000);

        // Claude Code hook alert (waiting for input / turn done / Bash failed)
        uint32_t evSeq = _link.clawEventSeq();
        bool eventActive = evSeq != 0 && evSeq != _dismissedEventSeq &&
                           (now - _link.clawEventRxMs() < EVENT_SHOW_MS);

        if (isPetted) {
            mood = "petted";
        } else if (eventActive) {
            mood = "ev_" + _link.clawEvent();
        }

        // Only redraw if mood changed, a new event arrived or animation frame ticked
        if (mood != _lastMood || isPetted || (eventActive && evSeq != _drawnEventSeq)) {
            _lastMood = mood;
            _drawnEventSeq = eventActive ? evSeq : 0;
            if (eventActive && !isPetted) {
                _drawEventBubble(tft, _link.clawEvent(), _link.clawEventText(), _link.clawEventProject());
            } else {
                _drawSpeechBubble(tft, mood);
            }
            _drawFacialExpressions(tft, mood);
        }

        // Animated leg movement (subtle 2px shift without wiping screen)
        _animateLegs(tft);
    }

private:
    static constexpr uint32_t REDRAW_INTERVAL_MS = 600;
    static constexpr uint32_t EVENT_SHOW_MS = 20000; // how long an alert stays on screen
    uint32_t _dismissedEventSeq = 0;
    uint32_t _drawnEventSeq = 0;
    PcLink &_link;
    uint32_t _lastDrawMs = 0;
    uint32_t _petTimerMs = 0;
    uint8_t _animFrame = 0;
    String _lastMood = "";

    // Official Color Palette 565
    static constexpr uint16_t COLOR_BODY = 0xE471;  // Salmon / Terracotta Pink (#E58C8A)
    static constexpr uint16_t COLOR_HAT  = 0xA59C;  // Lavender Blue (#A4B2E6)
    static constexpr uint16_t COLOR_STEM = 0xD67A;  // Light Grey / Silver Stem
    static constexpr uint16_t COLOR_BLUSH = 0xFB2F; // Soft pink cheeks

    void _drawSpeechBubble(Adafruit_ST7789 &tft, const String &mood) {
        // Speech bubble container (white)
        tft.fillRoundRect(20, 36, 200, 34, 6, ST77XX_WHITE);
        tft.fillTriangle(110, 70, 130, 70, 120, 76, ST77XX_WHITE);

        tft.setTextColor(ST77XX_BLACK);
        tft.setTextSize(2);

        if (mood == "petted") {
            tft.setCursor(35, 45);
            tft.print(" <3 PURR! <3 ");
        } else if (mood == "coding") {
            tft.setCursor(35, 45);
            tft.print("Ship code!! ");
        } else if (mood == "thinking") {
            tft.setCursor(35, 45);
            tft.print("Thinking... ");
        } else if (mood == "hyped") {
            tft.setCursor(35, 45);
            tft.print("LET'S BUILD!");
        } else if (mood == "sleeping") {
            tft.setCursor(60, 45);
            tft.print(" zZz... ");
        } else { // happy / idle
            tft.setCursor(40, 45);
            tft.print("Hola Alejo! ");
        }
    }

    // Coloured bubble for hook events: yellow = needs you, red = failure, green = done.
    void _drawEventBubble(Adafruit_ST7789 &tft, const String &ev, const String &text, const String &project) {
        uint16_t bg = ST77XX_GREEN;
        if (ev == "input") bg = ST77XX_YELLOW;
        else if (ev == "fail") bg = ST77XX_RED;
        uint16_t fg = (ev == "fail") ? ST77XX_WHITE : ST77XX_BLACK;

        tft.fillRoundRect(20, 36, 200, 34, 6, bg);
        tft.fillTriangle(110, 70, 130, 70, 120, 76, bg);
        tft.setTextColor(fg);

        String title = text.length() ? text : ev;
        if (title.length() > 16) title = title.substring(0, 16); // 16 x 12 px fits the bubble
        tft.setTextSize(2);
        tft.setCursor(120 - (int)title.length() * 6, project.length() ? 39 : 45);
        tft.print(title);

        if (project.length()) {
            String prj = project.length() > 32 ? project.substring(0, 32) : project;
            tft.setTextSize(1);
            tft.setCursor(120 - (int)prj.length() * 3, 59);
            tft.print(prj);
        }
    }

    void _drawStaticMascot(Adafruit_ST7789 &tft) {
        int centerX = 120;
        int headY = 135;

        // 1. Antenna Stem & Flower
        int starY = headY - 54;
        tft.drawLine(centerX, starY - 6, centerX, starY + 6, COLOR_HAT);
        tft.drawLine(centerX - 5, starY - 3, centerX + 5, starY + 3, COLOR_HAT);
        tft.drawLine(centerX - 5, starY + 3, centerX + 5, starY - 3, COLOR_HAT);
        tft.fillCircle(centerX, starY, 2, COLOR_HAT);

        tft.fillRect(centerX - 1, starY + 7, 3, 14, COLOR_STEM);

        // 2. Stepped Hat
        tft.fillRect(centerX - 12, headY - 33, 24, 10, COLOR_HAT);
        tft.fillRect(centerX - 24, headY - 23, 48, 12, COLOR_HAT);

        // 3. Main Body Box
        int bodyW = 110;
        int bodyH = 65;
        int bodyX = centerX - (bodyW / 2);
        tft.fillRect(bodyX, headY, bodyW, bodyH, COLOR_BODY);

        // 4. Side Ears
        tft.fillRect(bodyX - 10, headY + 22, 10, 18, COLOR_BODY);
        tft.fillRect(bodyX + bodyW, headY + 22, 10, 18, COLOR_BODY);
    }

    void _drawFacialExpressions(Adafruit_ST7789 &tft, const String &mood) {
        int centerX = 120;
        int headY = 135;

        // Clear face area inside body (salmon pink)
        tft.fillRect(centerX - 40, headY + 10, 80, 45, COLOR_BODY);
        // Clear floating heart area (black)
        tft.fillRect(centerX - 60, headY - 15, 20, 20, ST77XX_BLACK);
        tft.fillRect(centerX + 40, headY - 15, 20, 20, ST77XX_BLACK);

        if (mood == "ev_fail") {
            // X eyes + red "!" on both sides
            for (int d = -1; d <= 1; d++) {
                tft.drawLine(centerX - 28, headY + 20 + d, centerX - 12, headY + 36 + d, ST77XX_BLACK);
                tft.drawLine(centerX - 28, headY + 36 + d, centerX - 12, headY + 20 + d, ST77XX_BLACK);
                tft.drawLine(centerX + 12, headY + 20 + d, centerX + 28, headY + 36 + d, ST77XX_BLACK);
                tft.drawLine(centerX + 12, headY + 36 + d, centerX + 28, headY + 20 + d, ST77XX_BLACK);
            }
            _drawSideMarks(tft, "!", ST77XX_RED);
        } else if (mood == "ev_input") {
            // Friendly "looking at you" face: usual eyes raised a bit + rosy cheeks
            tft.fillRoundRect(centerX - 26, headY + 14, 10, 18, 3, ST77XX_BLACK);
            tft.fillRoundRect(centerX + 16, headY + 14, 10, 18, 3, ST77XX_BLACK);
            tft.fillRoundRect(centerX - 33, headY + 38, 14, 6, 3, COLOR_BLUSH);
            tft.fillRoundRect(centerX + 19, headY + 38, 14, 6, 3, COLOR_BLUSH);
            _drawSideMarks(tft, "?", ST77XX_YELLOW);
        } else if (mood == "ev_done") {
            // Happy eyes (^ ^), no hearts
            tft.drawLine(centerX - 28, headY + 32, centerX - 20, headY + 20, ST77XX_BLACK);
            tft.drawLine(centerX - 20, headY + 20, centerX - 12, headY + 32, ST77XX_BLACK);
            tft.drawLine(centerX + 12, headY + 32, centerX + 20, headY + 20, ST77XX_BLACK);
            tft.drawLine(centerX + 20, headY + 20, centerX + 28, headY + 32, ST77XX_BLACK);
        } else if (mood == "sleeping") {
            // Closed eyes (- -)
            tft.fillRect(centerX - 28, headY + 26, 16, 4, ST77XX_BLACK);
            tft.fillRect(centerX + 12, headY + 26, 16, 4, ST77XX_BLACK);
        } else if (mood == "petted" || mood == "hyped") {
            // Happy eyes (^ ^)
            tft.drawLine(centerX - 28, headY + 32, centerX - 20, headY + 20, ST77XX_BLACK);
            tft.drawLine(centerX - 20, headY + 20, centerX - 12, headY + 32, ST77XX_BLACK);

            tft.drawLine(centerX + 12, headY + 32, centerX + 20, headY + 20, ST77XX_BLACK);
            tft.drawLine(centerX + 20, headY + 20, centerX + 28, headY + 32, ST77XX_BLACK);

            // Floating hearts
            tft.setCursor(centerX - 58, headY - 10);
            tft.setTextColor(ST77XX_MAGENTA);
            tft.setTextSize(2);
            tft.print("<3");
            tft.setCursor(centerX + 42, headY - 10);
            tft.print("<3");
        } else {
            // Standard Official Black Vertical Rectangular Eyes
            int eyeW = 10;
            int eyeH = 22;
            tft.fillRect(centerX - 26, headY + 20, eyeW, eyeH, ST77XX_BLACK);
            tft.fillRect(centerX + 16, headY + 20, eyeW, eyeH, ST77XX_BLACK);
        }
    }

    void _drawSideMarks(Adafruit_ST7789 &tft, const char *mark, uint16_t color) {
        tft.setTextSize(2);
        tft.setTextColor(color);
        tft.setCursor(120 - 54, 135 - 12);
        tft.print(mark);
        tft.setCursor(120 + 46, 135 - 12);
        tft.print(mark);
    }

    void _animateLegs(Adafruit_ST7789 &tft) {
        int centerX = 120;
        int headY = 135;
        int bodyH = 65;
        int legW = 8;
        int legH = 16;
        int legY = headY + bodyH;

        int offset1 = (_animFrame == 0) ? 0 : 3;
        int offset2 = (_animFrame == 0) ? 3 : 0;

        // Clear leg bottom tips
        tft.fillRect(centerX - 40, legY + legH - 3, 80, 4, ST77XX_BLACK);

        // Draw active leg heights
        tft.fillRect(centerX - 35, legY, legW, legH - offset1, COLOR_BODY);
        tft.fillRect(centerX - 22, legY, legW, legH - offset2, COLOR_BODY);
        tft.fillRect(centerX + 14, legY, legW, legH - offset2, COLOR_BODY);
        tft.fillRect(centerX + 27, legY, legW, legH - offset1, COLOR_BODY);
    }
};
