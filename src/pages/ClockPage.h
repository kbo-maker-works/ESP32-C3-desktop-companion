#pragma once
#include "../core/Page.h"
#include "../core/NetManager.h"
#include "../config.h"
#include <time.h>
#include <math.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Clock page with 5 Watch Faces, integrated Weather complications + Vector Icons (Sun/Moon),
// and a 3D-Printed WordClock Replica ("Hablando Claro" Fuzzy Conversational Word Clock with Multi-Color LED words).
class ClockPage : public Page {
public:
    const char *name() const override { return "Reloj"; }

    void onEnter(Adafruit_ST7789 &tft) override {
        tft.fillScreen(ST77XX_BLACK);
        _lastDrawSec = -1;
        _lastDrawMin = -1;
        _lastFace = -1;
        _lastWordClockBucket = -1;
        _lastHeaderMin = -1;
        _prevSx = _prevSy = -1;
        _prevMx = _prevMy = -1;
        _prevHx = _prevHy = -1;
        _triedNtpSync = false;
        _maybeFetchWeather(true);
    }

    void onLongPress(Adafruit_ST7789 &tft) override {
        _watchFace = (_watchFace + 1) % 5; // Cycle 5 watch faces: 0, 1, 2, 3, 4
        tft.fillScreen(ST77XX_BLACK);
        _lastDrawSec = -1;
        _lastDrawMin = -1;
        _lastWordClockBucket = -1;
        _lastHeaderMin = -1;
        _prevSx = _prevSy = -1;
        _prevMx = _prevMy = -1;
        _prevHx = _prevHy = -1;
    }

    void update(Adafruit_ST7789 &tft) override {
        if (!_triedNtpSync && NetManager::isConnected()) {
            configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
            _triedNtpSync = true;
        }

        _maybeFetchWeather(false);

        time_t now = time(nullptr);
        struct tm tmNow;
        localtime_r(&now, &tmNow);

        if (tmNow.tm_sec == _lastDrawSec && _watchFace == _lastFace) return;
        _lastDrawSec = tmNow.tm_sec;
        _lastFace = _watchFace;

        switch (_watchFace) {
            case 0: _drawFuzzyWordClock(tft, tmNow); break;
            case 1: _drawStandardDigital(tft, tmNow); break;
            case 2: _drawGiantBoldDuoColor(tft, tmNow); break;
            case 3: _drawAnalogChronograph(tft, tmNow); break;
            case 4: _drawRetroStriped(tft, tmNow); break;
        }
    }

private:
    static constexpr uint32_t WEATHER_POLL_MS = 600000; // 10 minutes

    int _lastDrawSec = -1;
    int _lastDrawMin = -1;
    int _lastFace = -1;
    int _watchFace = 0;
    int _lastWordClockBucket = -1;
    int _lastHeaderMin = -1;
    bool _triedNtpSync = false;

    // Previous Analog Hand Coordinates
    int _prevSx = -1, _prevSy = -1;
    int _prevMx = -1, _prevMy = -1;
    int _prevHx = -1, _prevHy = -1;

    // Weather Complication State
    uint32_t _lastWeatherFetchMs = 0;
    bool _hasWeather = false;
    float _weatherTemp = 0.0f;
    int _weatherCode = 0;
    bool _isDay = true;
    String _weatherCond;

    // Palette
    static constexpr uint16_t COLOR_BLUE    = 0x04FF; // Electric Blue (#00A5FF)
    static constexpr uint16_t COLOR_CORAL   = 0xF980; // Vibrant Coral Red (#FF3B30)
    static constexpr uint16_t COLOR_GOLD    = 0xFBE0; // Warm Gold / Amber
    static constexpr uint16_t COLOR_CREAM   = 0xF77D; // Off-white cream
    static constexpr uint16_t COLOR_GREEN   = 0x07E0; // Electric Green
    static constexpr uint16_t COLOR_PINK    = 0xF81F; // Neon Pink / Magenta
    static constexpr uint16_t COLOR_CYAN    = 0x07FF; // Electric Cyan
    static constexpr uint16_t COLOR_ORANGE  = 0xFA20; // Bright Orange
    static constexpr uint16_t COLOR_PURPLE  = 0x981F; // Electric Purple

    // Spanish Localization Arrays
    const char *DAYS_ES[7]   = {"DOM", "LUN", "MAR", "MIE", "JUE", "VIE", "SAB"};
    const char *MONTHS_ES[12] = {"ENE", "FEB", "MAR", "ABR", "MAY", "JUN", "JUL", "AGO", "SEP", "OCT", "NOV", "DIC"};

    void _maybeFetchWeather(bool force) {
        if (!NetManager::isConnected()) return;
        uint32_t now = millis();
        if (!force && _lastWeatherFetchMs != 0 && (now - _lastWeatherFetchMs) < WEATHER_POLL_MS) {
            return;
        }
        _lastWeatherFetchMs = now;

        HTTPClient http;
        String url = String("http://api.open-meteo.com/v1/forecast?latitude=") +
                      OWM_LAT + "&longitude=" + OWM_LON +
                      "&current=temperature_2m,weather_code,is_day";

        http.useHTTP10(true);
        http.setTimeout(10000);
        http.setUserAgent("Mozilla/5.0 (ESP32-C3)");
        http.begin(url);
        int code = http.GET();

        if (code == 200) {
            String payload = http.getString();
            JsonDocument doc;
            if (!deserializeJson(doc, payload)) {
                _weatherTemp = doc["current"]["temperature_2m"].as<float>();
                _weatherCode = doc["current"]["weather_code"].as<int>();
                _isDay = doc["current"]["is_day"].as<int>() == 1;

                if (_weatherCode == 0) _weatherCond = "Despejado";
                else if (_weatherCode >= 1 && _weatherCode <= 3) _weatherCond = "Nublado";
                else if (_weatherCode == 45 || _weatherCode == 48) _weatherCond = "Niebla";
                else if (_weatherCode >= 51 && _weatherCode <= 67) _weatherCond = "Lluvia";
                else if (_weatherCode >= 71 && _weatherCode <= 77) _weatherCond = "Nieve";
                else if (_weatherCode >= 80 && _weatherCode <= 82) _weatherCond = "Chubascos";
                else if (_weatherCode >= 95) _weatherCond = "Tormenta";
                else _weatherCond = "Templado";

                _hasWeather = true;
            }
        }
        http.end();
    }

    void _drawWeatherIcon(Adafruit_ST7789 &tft, int x, int y) {
        if (!_hasWeather) return;

        static constexpr uint16_t COLOR_SUN_GOLD = 0xFD20; // Warm Solar Golden Yellow (#FFD700)

        if (_weatherCode == 0) {
            if (_isDay) {
                tft.fillCircle(x + 6, y + 6, 4, COLOR_SUN_GOLD);
                tft.drawLine(x + 6, y, x + 6, y + 2, COLOR_SUN_GOLD);
                tft.drawLine(x + 6, y + 10, x + 6, y + 12, COLOR_SUN_GOLD);
                tft.drawLine(x, y + 6, x + 2, y + 6, COLOR_SUN_GOLD);
                tft.drawLine(x + 10, y + 6, x + 12, y + 6, COLOR_SUN_GOLD);
            } else {
                tft.fillCircle(x + 6, y + 6, 5, COLOR_SUN_GOLD);
                tft.fillCircle(x + 8, y + 4, 5, ST77XX_BLACK);
            }
        } else if (_weatherCode >= 51 && _weatherCode <= 67) {
            tft.fillCircle(x + 4, y + 5, 3, 0xD67A);
            tft.fillCircle(x + 8, y + 4, 4, 0xD67A);
            tft.fillRect(x + 2, y + 6, 9, 3, 0xD67A);
            tft.drawLine(x + 4, y + 10, x + 2, y + 12, 0x07FF);
            tft.drawLine(x + 8, y + 10, x + 6, y + 12, 0x07FF);
        } else if (_weatherCode >= 95) {
            tft.fillCircle(x + 4, y + 4, 3, 0xD67A);
            tft.fillCircle(x + 8, y + 3, 4, 0xD67A);
            tft.fillRect(x + 2, y + 5, 9, 3, 0xD67A);
            tft.drawLine(x + 7, y + 8, x + 4, y + 11, ST77XX_YELLOW);
            tft.drawLine(x + 4, y + 11, x + 6, y + 13, ST77XX_YELLOW);
        } else {
            tft.fillCircle(x + 4, y + 5, 3, 0xD67A);
            tft.fillCircle(x + 8, y + 4, 4, 0xD67A);
            tft.fillRect(x + 2, y + 6, 9, 4, 0xD67A);
        }
    }

    String _getWeatherSummaryStr() const {
        if (!_hasWeather) return "";
        char buf[24];
        snprintf(buf, sizeof(buf), "%.1fC %s", _weatherTemp, _weatherCond.c_str());
        return String(buf);
    }

    // -----------------------------------------------------------------------
    // FACE 0: Standard Minimal Digital + Weather Icon & Condition
    // -----------------------------------------------------------------------
    void _drawStandardDigital(Adafruit_ST7789 &tft, const struct tm &tmNow) {
        char timeStr[12];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);

        char dateStr[20];
        snprintf(dateStr, sizeof(dateStr), "%02d-%02d-%04d", tmNow.tm_mday, tmNow.tm_mon + 1, tmNow.tm_year + 1900);

        if (NetManager::isApMode()) {
            tft.setTextColor(COLOR_GOLD, ST77XX_BLACK);
            tft.setTextSize(1);
            tft.setCursor(10, 10);
            tft.print("AP: Companion-Setup");
            tft.setCursor(10, 24);
            tft.print("IP: 192.168.4.1");
        } else {
            tft.setTextColor(NetManager::isConnected() ? ST77XX_GREEN : ST77XX_RED, ST77XX_BLACK);
            tft.setTextSize(1);
            tft.setCursor(15, 15);
            tft.print(NetManager::isConnected() ? "Wi-Fi OK" : "Sin Wi-Fi");
        }

        int timeX = (240 - ((int)strlen(timeStr) * 24)) / 2;
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft.setTextSize(4);
        tft.setCursor(timeX, 85);
        tft.print(timeStr);

        int dateX = (240 - ((int)strlen(dateStr) * 12)) / 2;
        tft.setTextColor(0x8410, ST77XX_BLACK);
        tft.setTextSize(2);
        tft.setCursor(dateX, 140);
        tft.print(dateStr);

        String wStr = _getWeatherSummaryStr();
        if (wStr.length() > 0) {
            int wX = (240 - (((int)wStr.length() * 12) + 24)) / 2;
            _drawWeatherIcon(tft, wX, 182);
            tft.setTextColor(COLOR_GREEN, ST77XX_BLACK);
            tft.setTextSize(2);
            tft.setCursor(wX + 24, 180);
            tft.print(wStr);
        }
    }

    // -----------------------------------------------------------------------
    // FACE 1: Apple / Nike Giant Bold Duo-Color + Larger Weather Icon & Text
    // -----------------------------------------------------------------------
    void _drawGiantBoldDuoColor(Adafruit_ST7789 &tft, const struct tm &tmNow) {
        char hrStr[4], minStr[4];
        snprintf(hrStr, sizeof(hrStr), "%02d", tmNow.tm_hour);
        snprintf(minStr, sizeof(minStr), "%02d", tmNow.tm_min);

        int hrX = (240 - ((int)strlen(hrStr) * 60)) / 2;
        tft.setTextColor(COLOR_BLUE, ST77XX_BLACK);
        tft.setTextSize(10);
        tft.setCursor(hrX, 10);
        tft.print(hrStr);

        int minX = (240 - ((int)strlen(minStr) * 60)) / 2;
        tft.setTextColor(COLOR_CORAL, ST77XX_BLACK);
        tft.setTextSize(10);
        tft.setCursor(minX, 115);
        tft.print(minStr);

        String wStr = _getWeatherSummaryStr();
        if (wStr.length() > 0) {
            int wX = (240 - (((int)wStr.length() * 12) + 20)) / 2;
            _drawWeatherIcon(tft, wX, 222);
            tft.setTextColor(COLOR_CREAM, ST77XX_BLACK);
            tft.setTextSize(2);
            tft.setCursor(wX + 20, 220);
            tft.print(wStr);
        }
    }

    // -----------------------------------------------------------------------
    // FACE 2: Classic Analog Chronograph + Weather Icon & Temp ONLY (Size 2)
    // -----------------------------------------------------------------------
    void _drawAnalogChronograph(Adafruit_ST7789 &tft, const struct tm &tmNow) {
        int cx = 120, cy = 120, r = 105;

        if (tmNow.tm_min != _lastDrawMin) {
            _lastDrawMin = tmNow.tm_min;
            tft.fillScreen(ST77XX_BLACK);

            for (int i = 0; i < 12; i++) {
                float angle = i * 30.0f * (M_PI / 180.0f);
                int x1 = cx + (r - 12) * sin(angle);
                int y1 = cy - (r - 12) * cos(angle);
                int x2 = cx + r * sin(angle);
                int y2 = cy - r * cos(angle);
                tft.drawLine(x1, y1, x2, y2, ST77XX_WHITE);
            }

            tft.drawCircle(75, 120, 22, 0x5AD6);
            tft.drawCircle(165, 120, 22, 0x5AD6);

            const char *dayTxt = DAYS_ES[tmNow.tm_wday];
            int dayX = 75 - ((strlen(dayTxt) * 6) / 2);
            tft.setTextColor(COLOR_CREAM);
            tft.setTextSize(1);
            tft.setCursor(dayX, 117);
            tft.print(dayTxt);

            char dateTxt[4];
            snprintf(dateTxt, sizeof(dateTxt), "%d", tmNow.tm_mday);
            int mX = 165 - ((strlen(dateTxt) * 6) / 2);
            tft.setCursor(mX, 117);
            tft.print(dateTxt);

            if (_hasWeather) {
                char tempStr[16];
                snprintf(tempStr, sizeof(tempStr), "%.1fC", _weatherTemp);
                int wX = 120 - ((((int)strlen(tempStr) * 12) + 20) / 2);
                _drawWeatherIcon(tft, wX, 175);
                tft.setTextColor(COLOR_GOLD);
                tft.setTextSize(2);
                tft.setCursor(wX + 20, 173);
                tft.print(tempStr);
            }

            _prevSx = _prevSy = -1;
            _prevMx = _prevMy = -1;
            _prevHx = _prevHy = -1;
        }

        float hourAngle = (tmNow.tm_hour % 12 + tmNow.tm_min / 60.0f) * 30.0f * (M_PI / 180.0f);
        float minAngle  = (tmNow.tm_min + tmNow.tm_sec / 60.0f) * 6.0f * (M_PI / 180.0f);
        float secAngle  = (tmNow.tm_sec) * 6.0f * (M_PI / 180.0f);

        int hx = cx + 55 * sin(hourAngle);
        int hy = cy - 55 * cos(hourAngle);

        int mx = cx + 78 * sin(minAngle);
        int my = cy - 78 * cos(minAngle);

        int sx = cx + 85 * sin(secAngle);
        int sy = cy - 85 * cos(secAngle);

        if (_prevSx != -1) {
            tft.drawLine(cx, cy, _prevSx, _prevSy, ST77XX_BLACK);
        }

        tft.drawLine(cx, cy, hx, hy, ST77XX_WHITE);
        tft.drawLine(cx+1, cy+1, hx+1, hy+1, ST77XX_WHITE);
        tft.drawLine(cx, cy, mx, my, ST77XX_WHITE);
        tft.drawLine(cx, cy, sx, sy, ST77XX_RED);
        tft.fillCircle(cx, cy, 3, ST77XX_RED);

        _prevSx = sx; _prevSy = sy;
        _prevMx = mx; _prevMy = my;
        _prevHx = hx; _prevHy = hy;
    }

    // -----------------------------------------------------------------------
    // FACE 3: Retro Striped / Art Deco Stacked
    // -----------------------------------------------------------------------
    void _drawRetroStriped(Adafruit_ST7789 &tft, const struct tm &tmNow) {
        char hrStr[4], minStr[4];
        snprintf(hrStr, sizeof(hrStr), "%02d", tmNow.tm_hour);
        snprintf(minStr, sizeof(minStr), "%02d", tmNow.tm_min);

        int hrX = (240 - ((int)strlen(hrStr) * 42)) / 2;
        tft.setTextColor(COLOR_CREAM, ST77XX_BLACK);
        tft.setTextSize(7);
        tft.setCursor(hrX, 15);
        tft.print(hrStr);

        int minX = (240 - ((int)strlen(minStr) * 42)) / 2;
        tft.setTextColor(COLOR_GOLD, ST77XX_BLACK);
        tft.setTextSize(7);
        tft.setCursor(minX, 105);
        tft.print(minStr);

        char dateStr[24];
        snprintf(dateStr, sizeof(dateStr), "%s * %s %d", DAYS_ES[tmNow.tm_wday], MONTHS_ES[tmNow.tm_mon], tmNow.tm_mday);

        int dateX = (240 - ((int)strlen(dateStr) * 12)) / 2;
        tft.setTextColor(0x8410, ST77XX_BLACK);
        tft.setTextSize(2);
        tft.setCursor(dateX, 182);
        tft.print(dateStr);

        String wStr = _getWeatherSummaryStr();
        if (wStr.length() > 0) {
            int wX = (240 - (((int)wStr.length() * 12) + 20)) / 2;
            _drawWeatherIcon(tft, wX, 215);
            tft.setTextColor(COLOR_GREEN, ST77XX_BLACK);
            tft.setTextSize(2);
            tft.setCursor(wX + 20, 213);
            tft.print(wStr);
        }
    }

    // -----------------------------------------------------------------------
    // FACE 0: 3D WordClock "Hablando Claro" (Fuzzy Dynamic Word Clock)
    // Phrases update in 5-minute buckets like a physical 3D Word Clock! Zero flicker.
    // -----------------------------------------------------------------------
    void _drawFuzzyWordClock(Adafruit_ST7789 &tft, const struct tm &tmNow) {
        int mBucket = tmNow.tm_min / 5;
        bool bucketChanged = (mBucket != _lastWordClockBucket);

        if (bucketChanged) {
            _lastWordClockBucket = mBucket;
            _lastDrawMin = tmNow.tm_min;
            tft.fillScreen(ST77XX_BLACK);
            
            // Generate & Render Fuzzy Conversational Time Phrase (only once per 5 min!)
            String phrase = _buildFuzzyPhrase(tmNow);
            _renderMultiColorWords(tft, phrase, tmNow.tm_hour, tmNow.tm_min);

            // Muted Dark Grey Footer Badge (Size 1)
            tft.setTextColor(0x4208, ST77XX_BLACK);
            tft.setTextSize(1);
            tft.setCursor(68, 222);
            tft.print("3D WORD CLOCK");
        }

        // Top Header: Centered Weather Text ONLY (Size 2)
        if (tmNow.tm_min != _lastHeaderMin || bucketChanged) {
            _lastHeaderMin = tmNow.tm_min;
            tft.fillRect(0, 0, 240, 36, ST77XX_BLACK);

            String wStr = _getWeatherSummaryStr();
            if (wStr.length() > 0) {
                int totalW = (wStr.length() * 12) + 20;
                int startX = (240 - totalW) / 2;

                _drawWeatherIcon(tft, startX, 8);
                tft.setTextColor(COLOR_GOLD, ST77XX_BLACK);
                tft.setTextSize(2);
                tft.setCursor(startX + 20, 8);
                tft.print(wStr);
            }
        }
    }

    String _buildFuzzyPhrase(const struct tm &tmNow) {
        int h = tmNow.tm_hour;
        int m = tmNow.tm_min;
        int bucket = m / 5;
        int varSeed = (h + bucket) % 3; // Stable seed per 5-min bucket!

        // --- SPECIAL TIME-OF-DAY CONTEXT RULES ---
        // 13:30 - 15:00 -> Lunch time! 🍕
        if (h == 13 && m >= 30) {
            if (varSeed == 0) return "HORA DE COMER ! 🍕";
            if (varSeed == 1) return "¡ A ALMORZAR ! 🍽️";
            return "¿ HAMBRE ? A COMER";
        }
        if (h == 14 && m <= 45) {
            if (varSeed == 0) return "HORA DEL ALMUERZO 🍝";
            if (varSeed == 1) return "A LA MESA YA ! 🍕";
            return "¿ YA HAS COMIDO ?";
        }

        // 17:00 - 18:30 -> End of work day! 🚗
        if ((h == 17 && m >= 0) || (h == 18 && m <= 30)) {
            if (varSeed == 0) return "YA ES HORA DE IRSE 🚗";
            if (varSeed == 1) return "¿ TRABAJO MAÑANA TAMBIEN ? 😅";
            return "CASI HORA DE PLEGAR";
        }

        // 08:00 - 09:15 -> Morning Coffee / Start ☕
        if ((h == 8 && m >= 0) || (h == 9 && m <= 15)) {
            if (varSeed == 0) return "¡ CAFE URGENTE ! ☕";
            if (varSeed == 1) return "HORA DE ARRANCAR 🚀";
            return "¡ BUENOS DIAS !";
        }

        // 23:30 - 06:45 -> Sleep time 😴
        if (h >= 23 || h <= 6) {
            if (varSeed == 0) return "¡ A DORMIR YA ! 😴";
            if (varSeed == 1) return "HORA DE LOS BUHOS 🦉";
            return "MAÑANA MADRUGAS ...";
        }

        // --- 5-MINUTE BUCKET FUZZY TIME PHRASES ---
        const char *HOURS_ES[12] = {
            "DOCE", "UNA", "DOS", "TRES", "CUATRO", "CINCO",
            "SEIS", "SIETE", "OCHO", "NUEVE", "DIEZ", "ONCE"
        };

        int h12 = h % 12;
        int nextH12 = (h + 1) % 12;
        const char *hStr = HOURS_ES[h12];
        const char *nextHStr = HOURS_ES[nextH12];

        switch (bucket) {
            case 0: // 00 - 04
                if (varSeed == 0) return String("SON LAS ") + hStr + " EN PUNTO";
                if (varSeed == 1) return String("EN PUNTO LAS ") + hStr;
                return String("JUSTO LAS ") + hStr;

            case 1: // 05 - 09
                if (varSeed == 0) return String("SON LAS ") + hStr + " Y CINCO";
                if (varSeed == 1) return String("PASADITAS LAS ") + hStr;
                return String("RECIEN LAS ") + hStr;

            case 2: // 10 - 14
                if (varSeed == 0) return String("SON LAS ") + hStr + " Y DIEZ";
                if (varSeed == 1) return String("PASADAS LAS ") + hStr;
                return String("LAS ") + hStr + " Y DIEZ";

            case 3: // 15 - 19
                if (varSeed == 0) return String("SON LAS ") + hStr + " Y CUARTO";
                if (varSeed == 1) return String("PASADAS LAS ") + hStr + " Y CUARTO";
                return String("LAS ") + hStr + " Y QUINCE";

            case 4: // 20 - 24
                if (varSeed == 0) return String("SON LAS ") + hStr + " Y VEINTE";
                if (varSeed == 1) return String("CAMINO A LA MEDIA DE ") + hStr;
                return String("LAS ") + hStr + " Y VEINTE";

            case 5: // 25 - 29
                if (varSeed == 0) return String("SON LAS ") + hStr + " Y VEINTICINCO";
                if (varSeed == 1) return String("CASI LAS ") + hStr + " Y MEDIA";
                return String("ROZANDO LA MEDIA DE ") + hStr;

            case 6: // 30 - 34
                if (varSeed == 0) return String("SON LAS ") + hStr + " Y MEDIA";
                if (varSeed == 1) return String("JUSTO LAS ") + hStr + " Y MEDIA";
                return String("MITAD DE LAS ") + hStr;

            case 7: // 35 - 39
                if (varSeed == 0) return String("PASADAS LAS ") + hStr + " Y MEDIA";
                if (varSeed == 1) return String("CAMINO A LAS ") + nextHStr;
                return String("PASA DE LAS ") + hStr + " Y MEDIA";

            case 8: // 40 - 44
                if (varSeed == 0) return String("FALTAN VEINTE PARA LAS ") + nextHStr;
                if (varSeed == 1) return String("MENOS VEINTE PARA ") + nextHStr;
                return String("CERCA DE LAS ") + nextHStr;

            case 9: // 45 - 49
                if (varSeed == 0) return String("FALTAN QUINCE PARA LAS ") + nextHStr;
                if (varSeed == 1) return String("MENOS CUARTO PARA ") + nextHStr;
                return String("CERCA DE LAS ") + nextHStr;

            case 10: // 50 - 54
                if (varSeed == 0) return String("FALTAN DIEZ PARA LAS ") + nextHStr;
                if (varSeed == 1) return String("CASI LAS ") + nextHStr;
                return String("ROZANDO LAS ") + nextHStr;

            case 11: // 55 - 59
            default:
                if (varSeed == 0) return String("SON CASI LAS ") + nextHStr;
                if (varSeed == 1) return String("POCO PARA LAS ") + nextHStr;
                return String("FALTA POCO PARA LAS ") + nextHStr;
        }
    }

    void _renderMultiColorWords(Adafruit_ST7789 &tft, const String &phrase, int hour, int min) {
        static constexpr uint16_t PALETTE[8] = {
            COLOR_GOLD, COLOR_PINK, COLOR_CYAN, COLOR_GREEN,
            COLOR_ORANGE, COLOR_PURPLE, COLOR_CREAM, COLOR_BLUE
        };

        String words[12];
        int wordCount = 0;
        int start = 0;

        for (int i = 0; i <= (int)phrase.length(); i++) {
            if (i == (int)phrase.length() || phrase[i] == ' ') {
                if (i > start) {
                    words[wordCount++] = phrase.substring(start, i);
                    if (wordCount >= 12) break;
                }
                start = i + 1;
            }
        }

        if (wordCount == 0) return;

        String lines[4];
        int lineCount = 0;
        String curLine = "";

        for (int i = 0; i < wordCount; i++) {
            if (curLine.isEmpty()) {
                curLine = words[i];
            } else if (curLine.length() + 1 + words[i].length() <= 13) {
                curLine += " " + words[i];
            } else {
                lines[lineCount++] = curLine;
                curLine = words[i];
                if (lineCount >= 3) break;
            }
        }
        if (!curLine.isEmpty() && lineCount < 4) {
            lines[lineCount++] = curLine;
        }

        int totalHeight = lineCount * 36;
        int startY = 55 + (140 - totalHeight) / 2;

        int wordIdxCounter = 0;
        for (int l = 0; l < lineCount; l++) {
            String lineStr = lines[l];
            int lineW = lineStr.length() * 18;
            int x = (240 - lineW) / 2;
            int y = startY + l * 36;

            int wStart = 0;
            for (int i = 0; i <= (int)lineStr.length(); i++) {
                if (i == (int)lineStr.length() || lineStr[i] == ' ') {
                    if (i > wStart) {
                        String w = lineStr.substring(wStart, i);
                        
                        uint32_t colorSeed = (hour * 7 + (min / 5) * 3 + wordIdxCounter * 5) % 8;
                        uint16_t wordColor = PALETTE[colorSeed];
                        wordIdxCounter++;

                        tft.setTextColor(wordColor, ST77XX_BLACK);
                        tft.setTextSize(3);
                        tft.setCursor(x, y);
                        tft.print(w);

                        x += w.length() * 18 + 18;
                    }
                    wStart = i + 1;
                }
            }
        }
    }
};
