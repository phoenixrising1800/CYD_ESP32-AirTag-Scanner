#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <set>
#include <vector>

// ── Touch pin config for ESP32 CYD ──────────────────────────────
//#define TOUCH_CS  33
//#define TOUCH_IRQ 36

TFT_eSPI tft = TFT_eSPI();

// Touchscreen pins
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
//XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define FONT_SIZE 2

// Touchscreen coordinates: (x, y) and pressure (z)
int x, y, z;

// ── Log buffer ───────────────────────────────────────────────────
struct LogLine {
    String text;
    uint16_t color;
};

const int MAX_LOG_LINES = 200;   // max lines stored in memory
const int LINE_HEIGHT   = 10;    // pixels per text line (font 1)
const int TOP_BAR_H     = 20;    // reserved for the counter bubble
int LINES_PER_SCREEN;            // computed in setup()

std::vector<LogLine> logBuffer;
int scrollOffset = 0;            // index of first visible line
bool needsRedraw = false;

// ── BLE state ────────────────────────────────────────────────────
int scanTime = 1;
BLEScan* pBLEScan;
std::set<String> foundDevices;
unsigned int airTagCount = 0;
String tagstatus = "";

// ── Helpers ──────────────────────────────────────────────────────

// Append a line to the log buffer and auto-scroll to bottom
void logLine(const String& text, uint16_t color = TFT_WHITE) {
    if (logBuffer.size() >= MAX_LOG_LINES) {
        logBuffer.erase(logBuffer.begin());   // drop oldest line
        if (scrollOffset > 0) scrollOffset--;
    }
    logBuffer.push_back({text, color});

    // Auto-scroll to bottom when new content arrives
    //int maxOffset = max(0, (int)logBuffer.size() - LINES_PER_SCREEN);
    //scrollOffset = maxOffset;
    needsRedraw = true;
}

void redrawLog() {
    // Clear only the log area (below the top bar)
    tft.fillRect(0, TOP_BAR_H, tft.width(), tft.height() - TOP_BAR_H, TFT_BLACK);

    tft.setTextFont(1);
    int y = TOP_BAR_H + 2;
    int end = min((int)logBuffer.size(), scrollOffset + LINES_PER_SCREEN);

    for (int i = scrollOffset; i < end; i++) {
        tft.setTextColor(logBuffer[i].color, TFT_BLACK);
        tft.setCursor(0, y);
        tft.print(logBuffer[i].text);
        y += LINE_HEIGHT;
    }

    needsRedraw = false;
}

void drawCounter() {
    int xCenter = tft.width() - 20;
    int yCenter = 10;
    int radius  = 9;
    tft.fillCircle(xCenter, yCenter, radius, TFT_YELLOW);
    tft.setTextColor(TFT_BLACK, TFT_YELLOW);
    tft.setTextFont(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(String(airTagCount), xCenter, yCenter);
    tft.setTextDatum(TL_DATUM);   // reset datum
}

// Draw a subtle scrollbar on the right edge
void drawScrollbar() {
    if (logBuffer.size() <= (size_t)LINES_PER_SCREEN) return;

    int barH   = tft.height() - TOP_BAR_H;
    int sbH    = max(10, (int)(barH * LINES_PER_SCREEN / (float)logBuffer.size()));
    int maxOff = logBuffer.size() - LINES_PER_SCREEN;
    int sbY    = TOP_BAR_H + (int)((barH - sbH) * scrollOffset / (float)maxOff);

    // track
    tft.drawFastVLine(tft.width() - 3, TOP_BAR_H, barH, TFT_DARKGREY);
    // thumb
    tft.drawFastVLine(tft.width() - 3, sbY, sbH, TFT_WHITE);
}

// ── Touch scrolling ──────────────────────────────────────────────
int  lastTouchY   = -1;
bool isTouching   = false;

void handleTouch() {
    if (!touchscreen.tirqTouched()) {
        if (isTouching) {
            isTouching = false;
            lastTouchY = -1;
        }
        return;
    }

    if (!touchscreen.touched()) return;

    TS_Point p = touchscreen.getPoint();
    // Map raw touch coords to screen pixels
    // Adjust these calibration values for your specific CYD panel if needed
    int screenX = map(p.y, 200, 3700, 0, SCREEN_WIDTH);
    int screenY = map(p.x, 240, 3800, 0, SCREEN_HEIGHT);
    //screenX = constrain(screenX, 0, tft.width()  - 1);
    //screenY = constrain(screenY, 0, tft.height() - 1);

    // ── Calibration debug — remove once values are confirmed ────
    //Serial.print("screenX: ");
    //Serial.print(screenX);
    //Serial.print("\t screenY: ");
    //Serial.print(screenY);
    //Serial.print("\n\t\tlastTouchY: ");
    //Serial.println(lastTouchY);
    // ────────────────────────────────────────────────────────────

    if (!isTouching) {
        isTouching = true;
        lastTouchY = screenY;
        return;
    }

    int dy = lastTouchY - screenY;   // positive = drag up = scroll down
    // // Test
    // if (dy > 0) { 
    //     Serial.print("dy: " + String(dy));
    //     Serial.println("\t\t\tPOSITIVE!! SCROLL DOWN HERE");
    // } else {
    //     Serial.print("dy: " + String(dy));
    //     Serial.println("\t\t\tNEGATIVE!! SCROLL UP HERE");
    // }
    // // ---
    lastTouchY = screenY;

    // Convert pixel drag to line scroll (1 line per LINE_HEIGHT pixels)
    int linesDelta = dy / LINE_HEIGHT;
    if (linesDelta == 0) return;

    int maxOffset = max(0, (int)logBuffer.size() - LINES_PER_SCREEN);
    scrollOffset  = constrain(scrollOffset + linesDelta, 0, maxOffset);
    needsRedraw   = true;
}

// ── BLE callback ─────────────────────────────────────────────────
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        uint8_t* payLoad       = advertisedDevice.getPayload();
        size_t   payLoadLength = advertisedDevice.getPayloadLength();
        String payload = "";


        bool patternFound = false;
        for (int i = 0; i <= (int)payLoadLength - 4; i++) {
            /// Testing for standardized Advertising packets with byte format [Length] FF [Company ID] [Company ID] 
            // if (payLoad[1] == 0xFF) {
            //     for (size_t i = 0; i < payLoadLength; i++) {
            //         payload += String(payLoad[i], HEX) + " ";
            //     }
            //     Serial.print("Matched: " ); Serial.println(payload);
            // }

            if (payLoad[i] == 0x4C && payLoad[i+1] == 0x00 && payLoad[i+2] == 0x07 && payLoad[i+3] == 0x19) {
                patternFound = true; tagstatus = "Registered "; break;
            }
            if (payLoad[i] == 0x4C && payLoad[i+1] == 0x00 && payLoad[i+2] == 0x12 && payLoad[i+3] == 0x19) {
                patternFound = true; tagstatus = "Unregistered "; break;
            }
            // Tile and Chipolo: only match within the first 4 bytes of the payload(??)
            // 0x75 0x00 (or 0x0075 standard for lookup) seems to be Samsung Electronics
            // 0x06 0x00 (or 0x0006 standard) seems to be Microsoft
            if (payLoad[1] == 0xFF) {
                if (payLoad[i+2] == 0xEC && payLoad[i+3] == 0xFE) { patternFound = true; tagstatus = "Tile ";    break; }
                if (payLoad[i+2] == 0x7C && payLoad[i+3] == 0x06) { patternFound = true; tagstatus = "Tile ";    break; }
                if (payLoad[i+2] == 0xED && payLoad[i+3] == 0xFE) { patternFound = true; tagstatus = "Tile ";    break; }
                if (payLoad[i+2] == 0x84 && payLoad[i+3] == 0xFD) { patternFound = true; tagstatus = "Tile ";    break; }
                if (payLoad[i+2] == 0x65 && payLoad[i+3] == 0xFE) { patternFound = true; tagstatus = "Chipolo "; break; }
                if (payLoad[i+2] == 0xC3 && payLoad[i+3] == 0x08) { patternFound = true; tagstatus = "Chipolo "; break; }
                if (payLoad[i+2] == 0x33 && payLoad[i+3] == 0xFE) { patternFound = true; tagstatus = "Chipolo "; break; }
            }
        }

        if (!patternFound) return;

        String macAddress = advertisedDevice.getAddress().toString().c_str();
        macAddress.toUpperCase();
        if (foundDevices.find(macAddress) != foundDevices.end()) return;

        foundDevices.insert(macAddress);
        airTagCount++;
        int rssi = advertisedDevice.getRSSI();

        // Serial output (unchanged)
        Serial.print(tagstatus); Serial.println("tracker found!");
        Serial.print("Tag: ");   Serial.println(airTagCount);
        Serial.print("MAC: ");   Serial.println(macAddress);
        Serial.print("RSSI: ");  Serial.print(rssi); Serial.println(" dBm");

    // Build payload hex string (first 5 bytes only)
    //size_t previewLen = min(payLoadLength, (size_t)5);  // use if only want 5 bytes in output
    //for (size_t i = 0; i < previewLen; i++) {
    for (size_t i = 0; i < payLoadLength; i++) {
        payload += String(payLoad[i], HEX) + " ";
    }
    //if (payLoadLength > 5) payload += "...";

        // Log to scrollable buffer
        logLine(String(airTagCount) + ": " + tagstatus + "Tag " + String(airTagCount) + ": " + macAddress, TFT_WHITE);
        logLine("\tRSSI: " + String(rssi) + " dBm", TFT_WHITE);
        logLine("Payload: " + payload, TFT_BLUE);
        logLine("", TFT_WHITE);   // blank separator
        //logLine("", TFT_WHITE);   // blank separator
    }
};

// ── Setup ────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // Start the SPI for the touchscreen and init the touchscreen
    touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touchscreen.begin(touchscreenSPI);
    // Set the TFT display rotation in non-landscape mode
    tft.setRotation(0);
    // Start the tft display
    tft.init();
    tft.fillScreen(TFT_BLACK);

    // Compute how many log lines fit below the top bar
    LINES_PER_SCREEN = (tft.height() - TOP_BAR_H) / LINE_HEIGHT;
    //logLine("LINES_PER_SCREEN: " + String(LINES_PER_SCREEN));

    //touchscreen.begin();
    //touchscreen.setRotation(0);   // match tft rotation

    logLine("Scanning for BT trackers...", TFT_WHITE);
    logLine("", TFT_WHITE);
    redrawLog();
    drawCounter();

    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    delay(1000);
}

// ── Loop ─────────────────────────────────────────────────────────
void loop() {
    // Serial rescan command
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        if (command.equals("rescan")) {
            foundDevices.clear();
            airTagCount = 0;
            logBuffer.clear();
            scrollOffset = 0;
            tft.fillScreen(TFT_BLACK);
            logLine("Rescanning for BT trackers...", TFT_WHITE);
            redrawLog();
        }
    }

    handleTouch();

    if (needsRedraw) {
        redrawLog();
        drawScrollbar();
        drawCounter();
    }

    pBLEScan->start(scanTime, false);
    pBLEScan->clearResults();
    delay(50);
}