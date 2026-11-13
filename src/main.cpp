#include <Arduino.h>
#include "config.h"
#include "core/Pins.h"
#include "core/DisplayManager.h"
#include "core/TouchButton.h"
#include "core/PageManager.h"
#include "core/NetManager.h"
#include "core/PcLink.h"
#include "pages/ClockPage.h"
#include "pages/WeatherPage.h"
#include "pages/ClaudeUsagePage.h"
#include "pages/ClawMascotPage.h"
#include "pages/FlightRadarPage.h"
#include "pages/MatrixRefresherPage.h"

DisplayManager display;
TouchButton touch;
PageManager pages;
PcLink pcLink;

ClockPage clockPage;
WeatherPage weatherPage;
ClaudeUsagePage claudeUsagePage(pcLink); // Toggles Claude <-> Antigravity on Long Press
ClawMascotPage clawMascotPage(pcLink);   // Official Claude Code robot mascot
FlightRadarPage flightRadarPage(pcLink); // Live ATC Vector Flight Radar Page
MatrixRefresherPage matrixPage;

void setup() {
    pcLink.begin(115200); // Serial (USB CDC) is used both for the PC link and for logs

    display.begin();
    touch.begin();
    NetManager::begin();

    pages.addPage(&clockPage);
    pages.addPage(&weatherPage);
    pages.addPage(&claudeUsagePage);
    pages.addPage(&clawMascotPage);
    pages.addPage(&flightRadarPage);
    pages.addPage(&matrixPage);
    pages.begin(display.tft());
}

void loop() {
    NetManager::loop();
    pcLink.update();
    touch.update();

    bool tapped = touch.wasTapped();
    bool longPressed = !tapped && touch.wasLongPressed();

    if (pages.isTemporary() && (tapped || longPressed)) {
        // Alert jump in progress: any touch dismisses it and goes back.
        clawMascotPage.dismissAlert();
        pages.returnFromTemporary(display.tft());
    } else if (tapped) {
        pages.next(display.tft());
    } else if (longPressed) {
        pages.longPress(display.tft());
    }

    // Claude Code needs you, finished, or a build failed: jump to the mascot,
    // then return to the previous page once the alert expires.
    static uint32_t lastClawEventSeq = 0;
    if (pcLink.clawEventSeq() != lastClawEventSeq) {
        lastClawEventSeq = pcLink.clawEventSeq();
        String ev = pcLink.clawEvent();
        if (ev == "input" || ev == "fail" || ev == "done") pages.showTemporarily(&clawMascotPage, display.tft());
    }
    if (pages.isTemporary() && !clawMascotPage.alertActive()) {
        pages.returnFromTemporary(display.tft());
    }

    pages.update(display.tft());
}
