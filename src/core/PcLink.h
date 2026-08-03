#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Reads newline-delimited JSON from Serial (USB CDC), sent by the
// pc-agent/companion_agent.py script. Expected line shape:
//   {"claude_tokens":142500,"claude_5h_pct":23.0,"claude_5h_reset":"29m",...}
class PcLink {
public:
    void begin(unsigned long baud = 115200) {
        Serial.setRxBufferSize(2048);
        Serial.begin(baud);
    }

    void update() {
        while (Serial.available()) {
            char c = Serial.read();
            if (c == '\n') {
                _parseLine(_lineBuf);
                _lineBuf = "";
            } else if (c != '\r') {
                _lineBuf += c;
                if (_lineBuf.length() > 1024) _lineBuf = ""; // guard against garbage
            }
        }
    }

    bool isOnline() const {
        return _lastRxMs != 0 && (millis() - _lastRxMs) < STALE_MS;
    }

    uint32_t claudeTokens() const { return _claudeTokens; }
    float claude5hPct() const { return _claude5hPct; }
    String claude5hReset() const { return _claude5hReset; }
    float claudeWeekPct() const { return _claudeWeekPct; }
    String claudeWeekReset() const { return _claudeWeekReset; }
    String claudeStatus() const { return _claudeStatus; }
    String clawMood() const { return _clawMood; }

    // Antigravity AI Assistant Metrics
    String agModel() const { return _agModel; }
    String agStatus() const { return _agStatus; }
    uint32_t agTokens() const { return _agTokens; }
    float agContextPct() const { return _agContextPct; }
    int agSubagents() const { return _agSubagents; }
    String agTool() const { return _agTool; }

private:
    static constexpr uint32_t STALE_MS = 20000; // 20 seconds stale timeout

    void _parseLine(const String &line) {
        if (line.isEmpty()) return;

        // Extract clean JSON payload substring {...}
        int startIdx = line.indexOf('{');
        int endIdx = line.lastIndexOf('}');
        if (startIdx < 0 || endIdx <= startIdx) return;
        String jsonStr = line.substring(startIdx, endIdx + 1);

        JsonDocument doc;
        if (deserializeJson(doc, jsonStr) != DeserializationError::Ok) return;

        // Parse Claude Fields (supports long & short keys)
        if (!doc["c_tok"].isNull()) _claudeTokens = doc["c_tok"].as<uint32_t>();
        else if (!doc["claude_tokens"].isNull()) _claudeTokens = doc["claude_tokens"].as<uint32_t>();

        if (!doc["c_5h"].isNull()) _claude5hPct = doc["c_5h"].as<float>();
        else if (!doc["claude_5h_pct"].isNull()) _claude5hPct = doc["claude_5h_pct"].as<float>();

        if (!doc["c_5hr"].isNull()) _claude5hReset = doc["c_5hr"].as<String>();
        else if (!doc["claude_5h_reset"].isNull()) _claude5hReset = doc["claude_5h_reset"].as<String>();

        if (!doc["c_wk"].isNull()) _claudeWeekPct = doc["c_wk"].as<float>();
        else if (!doc["claude_week_pct"].isNull()) _claudeWeekPct = doc["claude_week_pct"].as<float>();

        if (!doc["c_wkr"].isNull()) _claudeWeekReset = doc["c_wkr"].as<String>();
        else if (!doc["claude_week_reset"].isNull()) _claudeWeekReset = doc["claude_week_reset"].as<String>();

        if (!doc["c_st"].isNull()) _claudeStatus = doc["c_st"].as<String>();
        else if (!doc["claude_status"].isNull()) _claudeStatus = doc["claude_status"].as<String>();

        if (!doc["claw_mood"].isNull()) _clawMood = doc["claw_mood"].as<String>();

        // Parse Antigravity Fields (supports long & short keys)
        if (!doc["ag_m"].isNull()) _agModel = doc["ag_m"].as<String>();
        else if (!doc["ag_model"].isNull()) _agModel = doc["ag_model"].as<String>();

        if (!doc["ag_st"].isNull()) _agStatus = doc["ag_st"].as<String>();
        else if (!doc["ag_status"].isNull()) _agStatus = doc["ag_status"].as<String>();

        if (!doc["ag_tok"].isNull()) _agTokens = doc["ag_tok"].as<uint32_t>();
        else if (!doc["ag_tokens"].isNull()) _agTokens = doc["ag_tokens"].as<uint32_t>();

        if (!doc["ag_ctx"].isNull()) _agContextPct = doc["ag_ctx"].as<float>();
        else if (!doc["ag_context_pct"].isNull()) _agContextPct = doc["ag_context_pct"].as<float>();

        if (!doc["ag_sub"].isNull()) _agSubagents = doc["ag_sub"].as<int>();
        else if (!doc["ag_subagents"].isNull()) _agSubagents = doc["ag_subagents"].as<int>();

        if (!doc["ag_t"].isNull()) _agTool = doc["ag_t"].as<String>();
        else if (!doc["ag_tool"].isNull()) _agTool = doc["ag_tool"].as<String>();

        _lastRxMs = millis();
    }

    String _lineBuf;
    uint32_t _lastRxMs = 0;

    uint32_t _claudeTokens = 142500;
    float _claude5hPct = 23.0f;
    String _claude5hReset = "29m";
    float _claudeWeekPct = 17.0f;
    String _claudeWeekReset = "4d 22h";
    String _claudeStatus = "Idle";
    String _clawMood = "happy";

    // Antigravity defaults
    String _agModel = "Gemini 3.6 Flash";
    String _agStatus = "Thinking...";
    uint32_t _agTokens = 84200;
    float _agContextPct = 34.5f;
    int _agSubagents = 1;
    String _agTool = "replace_file";
};
