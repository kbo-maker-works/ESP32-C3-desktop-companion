#pragma once
#include <vector>
#include "Page.h"

// Cycles through a fixed list of Page* in order. Does not own/delete the
// pages (main.cpp keeps them as static globals for their whole lifetime).
class PageManager {
public:
    void addPage(Page *page) { _pages.push_back(page); }

    void begin(Adafruit_ST7789 &tft) {
        if (_pages.empty()) return;
        _pages[_index]->onEnter(tft);
    }

    void update(Adafruit_ST7789 &tft) {
        if (_pages.empty()) return;
        _pages[_index]->update(tft);
    }

    void next(Adafruit_ST7789 &tft) {
        if (_pages.empty()) return;
        _pages[_index]->onExit();
        _hasReturn = false; // manual navigation cancels any pending return
        _index = (_index + 1) % _pages.size();
        _pages[_index]->onEnter(tft);
    }

    // Temporarily jumps to a page (e.g. an alert), remembering the current one
    // so returnFromTemporary() can go back. No-op if that page is already showing.
    void showTemporarily(Page *page, Adafruit_ST7789 &tft) {
        for (size_t i = 0; i < _pages.size(); i++) {
            if (_pages[i] != page) continue;
            if (i == _index) return;
            if (!_hasReturn) {
                _returnIndex = _index;
                _hasReturn = true;
            }
            _switchTo(i, tft);
            return;
        }
    }

    bool isTemporary() const { return _hasReturn; }

    void returnFromTemporary(Adafruit_ST7789 &tft) {
        if (!_hasReturn) return;
        _hasReturn = false;
        _switchTo(_returnIndex, tft);
    }

    void longPress(Adafruit_ST7789 &tft) {
        if (_pages.empty()) return;
        _pages[_index]->onLongPress(tft);
    }

    const char *currentName() const {
        return _pages.empty() ? "none" : _pages[_index]->name();
    }

private:
    void _switchTo(size_t i, Adafruit_ST7789 &tft) {
        if (i == _index || i >= _pages.size()) return;
        _pages[_index]->onExit();
        _index = i;
        _pages[_index]->onEnter(tft);
    }

    std::vector<Page *> _pages;
    size_t _index = 0;
    size_t _returnIndex = 0;
    bool _hasReturn = false;
};
