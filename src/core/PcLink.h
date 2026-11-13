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
                if (_lineBuf.length() > 2048) _lineBuf = ""; // guard against garbage
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
    String claudeUser() const { return _claudeUser; }
    String clawMood() const { return _clawMood; }

    // Claude Code hook events ("input" / "done" / "fail"), see claude_events_hook.py.
    // clawEventSeq() changes every time a new event arrives (0 = none yet).
    uint32_t clawEventSeq() const { return _clawEventSeq; }
    uint32_t clawEventRxMs() const { return _clawEventRxMs; }
    String clawEvent() const { return _clawEvent; }
    String clawEventText() const { return _clawEventText; }
    String clawEventProject() const { return _clawEventProject; }

    // Antigravity AI Assistant Metrics
    String agModel() const { return _agModel; }
    String agStatus() const { return _agStatus; }
    uint32_t agTokens() const { return _agTokens; }
    float agContextPct() const { return _agContextPct; }
    int agSubagents() const { return _agSubagents; }
    String agTool() const { return _agTool; }

    // Flight Radar Metrics (Live FlightRadar24 ADS-B)
    struct FlightItem {
        String callsign;
        String airline;
        float dist;
        uint16_t heading;
        String dir;
        uint16_t alt;
        uint16_t spd;
        uint8_t category; // 0 = Passenger (Yellow), 1 = Military (Red), 2 = Cargo (Green)

        FlightItem() : callsign(""), airline(""), dist(0.0f), heading(0), dir("N"), alt(0), spd(0), category(0) {}
        FlightItem(String c, String a, float d, uint16_t h, String dr, uint16_t al, uint16_t sp, uint8_t cat = 0)
            : callsign(c), airline(a), dist(d), heading(h), dir(dr), alt(al), spd(sp), category(cat) {}

        bool isMilitary() const { return category == 1; }
        bool isCargo() const { return category == 2; }
    };

    bool flightActive() const { return _flightActive; }
    String flightCallsign() const { return _flightCallsign; }
    String flightAirline() const { return _flightAirline; }
    String flightType() const { return _flightType; }
    uint16_t flightAlt() const { return _flightAlt; }
    uint16_t flightSpd() const { return _flightSpd; }
    float flightDist() const { return _flightDist; }
    uint16_t flightHeading() const { return _flightHeading; }
    String flightDir() const { return _flightDir; }
    int flightCount() const { return _flightCount; }
    int flightItemCount() const { return _flightItemCount; }

    FlightItem getFlightItem(int idx) const {
        if (idx >= 0 && idx < _flightItemCount) return _flightItems[idx];
        uint8_t cat = (_flightType == "Militar") ? 1 : ((_flightType == "Cargo") ? 2 : 0);
        return FlightItem(_flightCallsign, _flightAirline, _flightDist, _flightHeading, _flightDir, _flightAlt, _flightSpd, cat);
    }

    uint32_t flightRevision() const { return _flightRevision; }

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

        if (!doc["c_user"].isNull()) _claudeUser = doc["c_user"].as<String>();
        else if (!doc["claude_user"].isNull()) _claudeUser = doc["claude_user"].as<String>();

        if (!doc["claw_mood"].isNull()) _clawMood = doc["claw_mood"].as<String>();

        if (!doc["claw_ev_id"].isNull()) {
            uint32_t id = doc["claw_ev_id"].as<uint32_t>();
            if (id != 0 && id != _clawEventId) {
                _clawEventId = id;
                _clawEvent = doc["claw_ev"] | "";
                _clawEventText = doc["claw_ev_txt"] | "";
                _clawEventProject = doc["claw_ev_prj"] | "";
                _clawEventRxMs = millis();
                _clawEventSeq++;
            }
        }

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

        // Parse Flight Radar Fields
        bool hasFlightData = false;
        if (!doc["fl_act"].isNull()) { _flightActive = doc["fl_act"].as<bool>(); hasFlightData = true; }
        if (!doc["fl_call"].isNull()) { _flightCallsign = doc["fl_call"].as<String>(); hasFlightData = true; }
        if (!doc["fl_air"].isNull()) { _flightAirline = doc["fl_air"].as<String>(); hasFlightData = true; }
        if (!doc["fl_type"].isNull()) { _flightType = doc["fl_type"].as<String>(); hasFlightData = true; }
        if (!doc["fl_alt"].isNull()) { _flightAlt = doc["fl_alt"].as<uint16_t>(); hasFlightData = true; }
        if (!doc["fl_spd"].isNull()) { _flightSpd = doc["fl_spd"].as<uint16_t>(); hasFlightData = true; }
        if (!doc["fl_dist"].isNull()) { _flightDist = doc["fl_dist"].as<float>(); hasFlightData = true; }
        if (!doc["fl_head"].isNull()) { _flightHeading = doc["fl_head"].as<uint16_t>(); hasFlightData = true; }
        if (!doc["fl_dir"].isNull()) { _flightDir = doc["fl_dir"].as<String>(); hasFlightData = true; }
        if (!doc["fl_cnt"].isNull()) { _flightCount = doc["fl_cnt"].as<int>(); hasFlightData = true; }

        // Parse up to 8 multi-flight entries
        int itemIdx = 0;
        for (int i = 1; i <= 8; i++) {
            String kCall = "fl_c" + String(i);
            if (!doc[kCall].isNull() && itemIdx < 8) {
                String kAir = "fl_a" + String(i);
                String kDist = "fl_d" + String(i);
                String kHead = "fl_h" + String(i);
                String kDir = "fl_dir" + String(i);
                String kAlt = "fl_alt" + String(i);
                String kSpd = "fl_spd" + String(i);
                String kMil = "fl_m" + String(i);
                String kType = "fl_t" + String(i);

                _flightItems[itemIdx].callsign = doc[kCall].as<String>();
                _flightItems[itemIdx].airline = doc[kAir].isNull() ? "Comercial" : doc[kAir].as<String>();
                _flightItems[itemIdx].dist = doc[kDist].isNull() ? 0.0f : doc[kDist].as<float>();
                _flightItems[itemIdx].heading = doc[kHead].isNull() ? 0 : doc[kHead].as<uint16_t>();
                _flightItems[itemIdx].dir = doc[kDir].isNull() ? "N" : doc[kDir].as<String>();
                _flightItems[itemIdx].alt = doc[kAlt].isNull() ? 0 : doc[kAlt].as<uint16_t>();
                _flightItems[itemIdx].spd = doc[kSpd].isNull() ? 0 : doc[kSpd].as<uint16_t>();
                
                uint8_t cat = 0;
                if (!doc[kType].isNull()) {
                    cat = doc[kType].as<uint8_t>();
                } else if (!doc[kMil].isNull() && doc[kMil].as<int>() == 1) {
                    cat = 1;
                }
                _flightItems[itemIdx].category = cat;
                itemIdx++;
                hasFlightData = true;
            }
        }
        if (hasFlightData) {
            _flightItemCount = itemIdx;
            _flightRevision++;
        }

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
    String _claudeUser = "";
    String _clawMood = "happy";

    uint32_t _clawEventId = 0;
    uint32_t _clawEventSeq = 0;
    uint32_t _clawEventRxMs = 0;
    String _clawEvent = "";
    String _clawEventText = "";
    String _clawEventProject = "";

    // Antigravity defaults
    String _agModel = "Gemini 3.6 Flash";
    String _agStatus = "Thinking...";
    uint32_t _agTokens = 84200;
    float _agContextPct = 34.5f;
    int _agSubagents = 1;
    String _agTool = "replace_file";

    // Flight Radar defaults
    bool _flightActive = false;
    String _flightCallsign = "IBE3150";
    String _flightAirline = "Iberia";
    String _flightType = "Comercial";
    uint16_t _flightAlt = 3200;
    uint16_t _flightSpd = 420;
    float _flightDist = 4.2f;
    uint16_t _flightHeading = 257;
    String _flightDir = "N";
    int _flightCount = 0;
    FlightItem _flightItems[8];
    int _flightItemCount = 0;
    uint32_t _flightRevision = 0;
};
