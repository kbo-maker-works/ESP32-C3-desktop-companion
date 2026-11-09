#pragma once
#include "../core/Page.h"
#include "../core/PcLink.h"

// AI Usage & Quotas Page: Toggles between CLAUDE CODE and ANTIGRAVITY AI on Long Press.
// Pure Pitch-Black OLED-style contrast with non-glaring muted palettes & zero-flicker rendering.
class ClaudeUsagePage : public Page {
public:
    explicit ClaudeUsagePage(PcLink &link) : _link(link) {}

    const char *name() const override {
        return _aiMode == 0 ? "Claude Code" : "Antigravity AI";
    }

    void onEnter(Adafruit_ST7789 &tft) override {
        _lastDrawMs = 0; // force immediate redraw
        _prevTokens = 0xFFFFFFFF;
        _prevPct1 = -1.0f;
        _prevPct2 = -1.0f;
        _prevStatus = "";
        _prevReset1 = "";
        _prevReset2 = "";
        _prevTool = "";
        _prevModel = "";
        _prevClaudeUser = "";
        _modeBannerMs = 0;
        _drawStaticLayout(tft);
    }

    void onLongPress(Adafruit_ST7789 &tft) override {
        // Toggle between 0: CLAUDE CODE and 1: ANTIGRAVITY AI
        _aiMode = (_aiMode + 1) % 2;
        _modeBannerMs = millis();
        _lastDrawMs = 0; // force immediate redraw
        _drawStaticLayout(tft);
        _drawToast(tft);
    }

    void update(Adafruit_ST7789 &tft) override {
        uint32_t now = millis();

        // Clear toast banner after 2 seconds
        if (_modeBannerMs > 0 && (now - _modeBannerMs) > 2000) {
            _modeBannerMs = 0;
            _drawStaticLayout(tft);
        }

        if (now - _lastDrawMs < REDRAW_INTERVAL_MS) return;
        _lastDrawMs = now;

        bool online = _link.isOnline();

        if (_aiMode == 0) {
            _updateClaudeMode(tft, online);
        } else {
            _updateAntigravityMode(tft, online);
        }
    }

private:
    static constexpr uint32_t REDRAW_INTERVAL_MS = 250;

    // Pure Pitch-Black Base Palette
    static constexpr uint16_t COLOR_BG           = 0x0000; // True Pitch Black (#000000)
    static constexpr uint16_t COLOR_CREAM        = 0xFFDC; // Crisp Soft Cream (#F5F5F0)
    static constexpr uint16_t COLOR_AMBER        = 0xFBE0; // Warm Amber / Gold (#F59E0B)

    // Claude Palette
    static constexpr uint16_t COLOR_CLAUDE_HDR   = 0x0841; // Deep Charcoal (#09090C)
    static constexpr uint16_t COLOR_CLAUDE_CARD  = 0x0861; // Dark Slate Card (#0D0D10)
    static constexpr uint16_t COLOR_CLAUDE_LINE  = 0x18C3; // Fine Border (#1C1C22)
    static constexpr uint16_t COLOR_TERRACOTTA   = 0xDBCA; // Anthropic Claude Terracotta (#D97757)
    static constexpr uint16_t COLOR_LAVENDER     = 0xA59C; // Anthropic Lavender Blue (#A4B2E6)
    static constexpr uint16_t COLOR_CLAUDE_MUTED = 0x6CF9; // Muted Silver (#6B7280)

    // Antigravity Palette (Muted, Non-Glaring Steel Cyan & DeepMind Emerald)
    static constexpr uint16_t COLOR_AG_HDR       = 0x0883; // Dark Space Charcoal (#0D1117)
    static constexpr uint16_t COLOR_AG_CARD      = 0x0883; // Deep Charcoal Slate (#0D1117)
    static constexpr uint16_t COLOR_AG_LINE      = 0x1D67; // Muted Slate Border (#1F2937)
    static constexpr uint16_t COLOR_STEEL_CYAN    = 0x0D5D; // Muted Steel Cyan (#0EA5E9)
    static constexpr uint16_t COLOR_AG_TEAL       = 0x15D4; // Soft DeepMind Teal (#14B8A6)
    static constexpr uint16_t COLOR_AG_MUTED      = 0x63AC; // Soft Slate Grey (#64748B)

    PcLink &_link;
    uint32_t _lastDrawMs = 0;
    uint32_t _modeBannerMs = 0;
    uint8_t _aiMode = 0; // 0 = Claude Code, 1 = Antigravity AI

    uint32_t _prevTokens = 0xFFFFFFFF;
    float _prevPct1 = -1.0f;
    float _prevPct2 = -1.0f;
    String _prevStatus;
    String _prevReset1;
    String _prevReset2;
    String _prevTool;
    String _prevModel;
    String _prevClaudeUser;

    void _drawStaticLayout(Adafruit_ST7789 &tft) {
        tft.fillScreen(COLOR_BG);

        if (_aiMode == 0) {
            // CLAUDE CODE LAYOUT
            tft.fillRect(0, 0, 240, 26, COLOR_CLAUDE_HDR);
            _drawAnthropicLogo(tft, 20, 13);
            tft.setTextColor(COLOR_CREAM);
            tft.setTextSize(2);
            tft.setCursor(38, 5);
            tft.print("CLAUDE CODE");
            tft.fillRect(0, 26, 240, 2, COLOR_TERRACOTTA);

            tft.setTextColor(COLOR_CLAUDE_MUTED, COLOR_BG);
            tft.setTextSize(1);
            tft.setCursor(12, 48);
            tft.print("TOKENS USADOS");

            _drawCardWireframe(tft, 8, 92, 224, 68, "SESION", COLOR_TERRACOTTA, COLOR_CLAUDE_CARD, COLOR_CLAUDE_LINE);
            _drawCardWireframe(tft, 8, 164, 224, 68, "SEMANAL", COLOR_LAVENDER, COLOR_CLAUDE_CARD, COLOR_CLAUDE_LINE);
        } else {
            // ANTIGRAVITY AI LAYOUT
            tft.fillRect(0, 0, 240, 26, COLOR_AG_HDR);
            _drawAntigravityLogo(tft, 18, 13);
            tft.setTextColor(COLOR_CREAM);
            tft.setTextSize(2);
            tft.setCursor(34, 5);
            tft.print("ANTIGRAVITY");
            tft.fillRect(0, 26, 240, 2, COLOR_STEEL_CYAN);

            tft.setTextColor(COLOR_AG_MUTED, COLOR_BG);
            tft.setTextSize(1);
            tft.setCursor(12, 48);
            tft.print("TOKENS AGY");

            // Swapped Card Order:
            // Card 1 (Top, Y=92): SUBAGENTES
            _drawCardWireframe(tft, 8, 92, 224, 68, "SUBAGENTES", COLOR_AG_TEAL, COLOR_AG_CARD, COLOR_AG_LINE);

            // Card 2 (Bottom, Y=164): CUOTA AGY
            _drawCardWireframe(tft, 8, 164, 224, 68, "CUOTA AGY", COLOR_STEEL_CYAN, COLOR_AG_CARD, COLOR_AG_LINE);
        }

        // Force full refresh
        _prevTokens = 0xFFFFFFFF;
        _prevPct1 = -1.0f;
        _prevPct2 = -1.0f;
        _prevStatus = "";
        _prevReset1 = "";
        _prevReset2 = "";
        _prevTool = "";
        _prevModel = "";
        _prevClaudeUser = "";

        // Synchronously populate full content in single pass
        bool online = _link.isOnline();
        if (_aiMode == 0) {
            _updateClaudeMode(tft, online);
        } else {
            _updateAntigravityMode(tft, online);
        }
    }

    void _drawToast(Adafruit_ST7789 &tft) {
        uint16_t toastBg = (_aiMode == 0) ? COLOR_CLAUDE_HDR : COLOR_AG_HDR;
        uint16_t toastBorder = (_aiMode == 0) ? COLOR_TERRACOTTA : COLOR_STEEL_CYAN;

        tft.fillRoundRect(15, 95, 210, 48, 8, toastBg);
        tft.drawRoundRect(15, 95, 210, 48, 8, toastBorder);
        tft.setTextColor(COLOR_CREAM, toastBg);
        tft.setTextSize(1);
        tft.setCursor(30, 104);
        tft.print("MODO IA CAMBIADO (LONG PRESS)");

        tft.setTextSize(2);
        tft.setTextColor(toastBorder, toastBg);
        tft.setCursor(30, 120);
        tft.print(_aiMode == 0 ? "CLAUDE CODE" : "ANTIGRAVITY AI");
    }

    void _drawAnthropicLogo(Adafruit_ST7789 &tft, int cx, int cy) {
        tft.fillCircle(cx, cy, 3, COLOR_TERRACOTTA);
        tft.drawLine(cx - 6, cy, cx + 6, cy, COLOR_TERRACOTTA);
        tft.drawLine(cx, cy - 6, cx, cy + 6, COLOR_TERRACOTTA);
    }

    void _drawAntigravityLogo(Adafruit_ST7789 &tft, int cx, int cy) {
        tft.drawCircle(cx, cy, 7, COLOR_STEEL_CYAN);
        tft.fillCircle(cx, cy, 2, COLOR_AG_TEAL);
    }

    void _drawCardWireframe(Adafruit_ST7789 &tft, int x, int y, int w, int h, const char *title,
                            uint16_t accentColor, uint16_t cardBg, uint16_t borderCol) {
        tft.fillRoundRect(x, y, w, h, 6, cardBg);
        tft.drawRoundRect(x, y, w, h, 6, borderCol);

        // Card Title (Size 2)
        tft.setTextColor(accentColor, cardBg);
        tft.setTextSize(2);
        tft.setCursor(x + 10, y + 8);
        tft.print(title);

        // Progress bar track (Thick 14px)
        tft.drawRoundRect(x + 10, y + 28, w - 20, 14, 3, borderCol);

        // Sub-label
        tft.setTextColor(_aiMode == 0 ? COLOR_CLAUDE_MUTED : COLOR_AG_MUTED, cardBg);
        tft.setTextSize(1);
        tft.setCursor(x + 10, y + 48);
        if (_aiMode == 0) {
            tft.print("Reset:");
        } else {
            if (strcmp(title, "CUOTA AGY") == 0) {
                tft.print("Tool:");
            } else {
                tft.print("Estado:");
            }
        }
    }

    void _updateClaudeMode(Adafruit_ST7789 &tft, bool online) {
        // 1. Status Line
        String status = online ? _link.claudeStatus() : "";
        if (status != _prevStatus) {
            _prevStatus = status;
            tft.fillRect(10, 30, 224, 18, COLOR_BG);

            if (!status.isEmpty() && status != "Idle" && status != "Desconectado") {
                uint16_t dotColor = COLOR_TERRACOTTA;
                tft.fillCircle(14, 37, 4, dotColor);
                tft.setTextColor(dotColor, COLOR_BG);
                tft.setTextSize(2);
                tft.setCursor(24, 30);
                tft.print(status);
            }
        }

        // 2. Logged-in User / Account Email (Right next to TOKENS USADOS in COLOR_TERRACOTTA)
        String user = online ? _link.claudeUser() : "";
        if (user != _prevClaudeUser) {
            _prevClaudeUser = user;
            tft.fillRect(94, 46, 138, 12, COLOR_BG);
            if (!user.isEmpty()) {
                if (user.length() > 22) user = user.substring(0, 20) + "..";
                int txtW = user.length() * 6;
                int txtX = 228 - txtW;
                if (txtX < 94) txtX = 94;
                tft.setTextColor(COLOR_TERRACOTTA, COLOR_BG);
                tft.setTextSize(1);
                tft.setCursor(txtX, 48);
                tft.print(user);
            }
        }

        // 3. Tokens Spent Value (0 when offline)
        uint32_t tokens = online ? _link.claudeTokens() : 0;
        if (tokens != _prevTokens) {
            _prevTokens = tokens;
            char tokStr[20];
            if (tokens >= 1000000) {
                snprintf(tokStr, sizeof(tokStr), "%.2f M", tokens / 1000000.0f);
            } else if (tokens >= 1000) {
                snprintf(tokStr, sizeof(tokStr), "%.1f K", tokens / 1000.0f);
            } else {
                snprintf(tokStr, sizeof(tokStr), "%lu", (unsigned long)tokens);
            }

            tft.fillRect(12, 60, 216, 26, COLOR_BG);
            tft.setTextColor(online ? COLOR_CREAM : COLOR_CLAUDE_MUTED, COLOR_BG);
            tft.setTextSize(3);
            tft.setCursor(12, 60);
            tft.print(tokStr);
        }

        // 3. Card 1: 5-Hour / Session Quota Bar (0% when offline)
        float pct1 = online ? _link.claude5hPct() : 0.0f;
        String rst1 = online ? _link.claude5hReset() : "--";
        _updateCardBar(tft, 8, 92, 224, 68,
                       "SESION", pct1, rst1,
                       COLOR_TERRACOTTA, COLOR_CLAUDE_CARD, COLOR_CLAUDE_LINE,
                       _prevPct1, _prevReset1);

        // 4. Card 2: Weekly Quota Bar (0% when offline)
        float pct2 = online ? _link.claudeWeekPct() : 0.0f;
        String rst2 = online ? _link.claudeWeekReset() : "--";
        _updateCardBar(tft, 8, 164, 224, 68,
                       "SEMANAL", pct2, rst2,
                       COLOR_LAVENDER, COLOR_CLAUDE_CARD, COLOR_CLAUDE_LINE,
                       _prevPct2, _prevReset2);
    }

    void _updateAntigravityMode(Adafruit_ST7789 &tft, bool online) {
        // 1. Status & Active Model Display
        String status = online ? _link.agStatus() : "Desconectado";
        String model = online ? _link.agModel() : "--";
        if (online && model.isEmpty()) model = "Gemini 3.6 Pro";

        if (status != _prevStatus || model != _prevModel) {
            _prevStatus = status;
            _prevModel = model;
            uint16_t dotColor = !online ? COLOR_AG_MUTED : COLOR_STEEL_CYAN;
            
            // Status Line
            tft.fillRect(24, 30, 210, 18, COLOR_BG);
            tft.fillCircle(14, 37, 4, dotColor);
            tft.setTextColor(dotColor, COLOR_BG);
            tft.setTextSize(2);
            tft.setCursor(24, 30);
            tft.print(status);
        }

        // 2. Antigravity Tokens Spent Value & Model Badge (0 when offline)
        uint32_t tokens = online ? _link.agTokens() : 0;
        if (tokens != _prevTokens) {
            _prevTokens = tokens;
            char tokStr[20];
            if (tokens >= 1000000) {
                snprintf(tokStr, sizeof(tokStr), "%.2f M", tokens / 1000000.0f);
            } else if (tokens >= 1000) {
                snprintf(tokStr, sizeof(tokStr), "%.1f K", tokens / 1000.0f);
            } else {
                snprintf(tokStr, sizeof(tokStr), "%lu", (unsigned long)tokens);
            }

            tft.fillRect(12, 60, 216, 26, COLOR_BG);
            tft.setTextColor(online ? COLOR_CREAM : COLOR_AG_MUTED, COLOR_BG);
            tft.setTextSize(3);
            tft.setCursor(12, 60);
            tft.print(tokStr);

            // Render Model Pill Tag at Top Right (e.g. "Gemini 3.6 Pro")
            tft.fillRoundRect(138, 48, 94, 18, 4, COLOR_AG_CARD);
            tft.drawRoundRect(138, 48, 94, 18, 4, COLOR_AG_LINE);
            tft.setTextColor(online ? COLOR_STEEL_CYAN : COLOR_AG_MUTED, COLOR_AG_CARD);
            tft.setTextSize(1);
            tft.setCursor(144, 53);
            tft.print(model.substring(0, 13));
        }

        // 3. Card 1 (Top Card, Y=92): SUBAGENTES
        int subagents = online ? _link.agSubagents() : 0;
        float subPct = (float)(subagents * 25.0f); // 1 agent = 25%, 4 agents = 100%
        String subStr = String(subagents) + (subagents == 1 ? " activo" : " activos");
        _updateCardBar(tft, 8, 92, 224, 68,
                       "SUBAGENTES", subPct, subStr,
                       COLOR_AG_TEAL, COLOR_AG_CARD, COLOR_AG_LINE,
                       _prevPct2, _prevReset2);

        // 4. Card 2 (Bottom Card, Y=164): CUOTA AGY & TOOL
        float agPct = online ? _link.agContextPct() : 0.0f;
        String toolStr = online ? _link.agTool() : "--";
        _updateCardBar(tft, 8, 164, 224, 68,
                       "CUOTA AGY", agPct, toolStr,
                       COLOR_STEEL_CYAN, COLOR_AG_CARD, COLOR_AG_LINE,
                       _prevPct1, _prevTool);
    }

    void _updateCardBar(Adafruit_ST7789 &tft, int x, int y, int w, int h,
                        const char *title, float pct, const String &subText,
                        uint16_t accentColor, uint16_t cardBg, uint16_t borderCol,
                        float &prevPct, String &prevSubText) {
        pct = pct < 0.0f ? 0.0f : (pct > 100.0f ? 100.0f : pct);

        // Percentage label (% USADO) at Top-Right
        if (pct != prevPct) {
            char pctBuf[16];
            snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", pct);
            
            tft.fillRect(x + w - 75, y + 6, 65, 18, cardBg);
            tft.setTextColor(COLOR_CREAM, cardBg);
            tft.setTextSize(2);
            int txtX = (x + w - 10) - ((int)strlen(pctBuf) * 12);
            tft.setCursor(txtX, y + 8);
            tft.print(pctBuf);

            int barX = x + 10;
            int barY = y + 28;
            int barW = w - 20;
            int barH = 14;

            int fillW = (int)((barW - 4) * (pct / 100.0f));
            tft.fillRect(barX + 2, barY + 2, barW - 4, barH - 4, cardBg);
            if (fillW > 0) {
                if (fillW < 6) {
                    tft.fillRect(barX + 2, barY + 2, fillW, barH - 4, accentColor);
                } else {
                    tft.fillRoundRect(barX + 2, barY + 2, fillW, barH - 4, 2, accentColor);
                }
            }
            prevPct = pct;
        }

        // Sub-text Label at Bottom (Tool: / Subagentes: / Reset:)
        if (subText != prevSubText) {
            prevSubText = subText;
            tft.fillRect(x + 50, y + 48, 160, 14, cardBg);
            tft.setTextColor(COLOR_CREAM, cardBg);
            tft.setTextSize(1);
            tft.setCursor(x + 52, y + 48);
            tft.print(subText);
        }
    }
};
