// ═══════════════════════════════════════════════════════
//  ui.ino — All screen drawing for CYD 320x240
//  Flicker-free dashboard: static chrome drawn once,
//  dynamic regions overwritten in place.
// ═══════════════════════════════════════════════════════

#define C_BG      TFT_BLACK
#define C_TEXT    TFT_WHITE
#define C_DIM     0x7BEF
#define C_BAR_BG  0x2104
#define C_OK      0x07E0
#define C_WARN    0xFD20
#define C_CRIT    0xF800
#define C_HEAD    0xEB87
#define C_ACCENT  0xEB87
#define C_CYAN    0xF50A

// ── Layout (everything nudged up ~20% from original) ──
//  Header:       y=0   h=24
//  5H label:     y=30
//  5H bar:       y=42  h=18
//  7D label:     y=70
//  7D bar:       y=82  h=18
//  Rst labels:   y=108
//  Rst values:   y=122
//  Divider:      y=152
//  Big lbl:      y=158
//  Big val:      y=168
//  Footer:       y=224 h=16

#define HDR_H    24
#define BAR_H    22
#define FOOTER_Y (SCREEN_H - 16)

static bool dashboardDrawn = false;
static float    lastH5 = -1, lastD7 = -1;
static uint32_t lastH5Reset = 0, lastD7Reset = 0;
static bool     lastOk = true;

static uint16_t barColor(float pct) {
    if (pct > 80.0f) return C_CRIT;
    if (pct > 50.0f) return C_WARN;
    return C_OK;
}

static void fmtCountdown(uint32_t epoch, char* out, size_t len) {
    if (epoch == 0) { strlcpy(out, "--", len); return; }
    time_t now; time(&now);
    int32_t diff = (int32_t)epoch - (int32_t)now;
    if (diff <= 0) { strlcpy(out, "now", len); return; }
    int d = diff / 86400, h = (diff % 86400) / 3600, m = (diff % 3600) / 60;
    if (d > 0)      snprintf(out, len, "%dd%dh", d, h);
    else if (h > 0) snprintf(out, len, "%dh%02dm", h, m);
    else            snprintf(out, len, "%dm", m);
}

static void updateBar(int x, int y, int w, int h, float pct, uint16_t col) {
    int fw = constrain((int)(w * pct / 100.0f), 0, w);
    if (fw > 0) lcd.fillRect(x, y, fw, h, col);
    if (fw < w) lcd.fillRect(x + fw, y, w - fw, h, C_BAR_BG);
}

static void updateText(int x, int y, int clearW, int clearH,
                       const char* str, uint16_t col, uint8_t sz) {
    lcd.fillRect(x, y, clearW, clearH, C_BG);
    lcd.setTextColor(col, C_BG);
    lcd.setTextSize(sz);
    lcd.setCursor(x, y);
    lcd.print(str);
}

// ── Static chrome — drawn once ─────────────────────────
static void drawDashboardChrome() {
    lcd.fillScreen(C_BG);

    // Header
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, C_HEAD);
    lcd.setTextColor(C_BG, C_HEAD);
    lcd.setTextSize(1);
    lcd.setCursor(6, 8);
    lcd.print("CLAUDE USAGE");

    int barW = SCREEN_W - 24;

    // Bar labels — size 1
    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(12, 30); lcd.print("5-HOUR WINDOW");
    lcd.setCursor(12, 70); lcd.print("7-DAY WINDOW");

    // Bar tracks
    lcd.fillRect(12, 40, barW, BAR_H, C_BAR_BG);
    lcd.fillRect(12, 80, barW, BAR_H, C_BAR_BG);

    // Countdown labels — size 1
    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(12,              114); lcd.print("5H RESETS IN");
    lcd.setCursor(SCREEN_W/2 + 12, 114); lcd.print("7D RESETS IN");

    // Divider
    lcd.drawFastHLine(12, 158, SCREEN_W - 24, C_DIM);

    // Big number labels — size 1
    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(12,              164); lcd.print("5H USED");
    lcd.setCursor(SCREEN_W/2 + 12, 164); lcd.print("7D USED");

    // Footer
    lcd.fillRect(0, FOOTER_Y, SCREEN_W, 16, 0x1082);
    lcd.setTextColor(C_DIM, 0x1082);
    lcd.setTextSize(1);
    lcd.setCursor(6, FOOTER_Y + 5);
    lcd.print("BOOT=brightness  BTN=refresh now");

    dashboardDrawn = true;
}

// ── Public UI functions ────────────────────────────────

void uiInit() {
    lcd.setRotation(SCREEN_ROT);
    lcd.fillScreen(C_BG);
    dashboardDrawn = false;
    lastH5 = -1; lastD7 = -1;
}

void uiBootProgress(int percent, const char* label) {
    dashboardDrawn = false;
    lcd.fillScreen(C_BG);

    lcd.setTextColor(C_ACCENT, C_BG);
    lcd.setTextSize(2);
    lcd.setCursor(60, 30);
    lcd.print("Claude Usage");

    int bx = 30, by = 80, bw = SCREEN_W - 60, bh = 16;
    lcd.fillRect(bx, by, bw, bh, C_BAR_BG);
    int fill = constrain((int)(bw * percent / 100.0f), 0, bw);
    lcd.fillRect(bx, by, fill, bh, C_ACCENT);

    char ps[8];
    snprintf(ps, sizeof(ps), "%d%%", percent);
    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(bx + bw / 2 - (int)strlen(ps) * 3, by + bh + 8);
    lcd.print(ps);

    lcd.setCursor(30, 125);
    lcd.setTextColor(C_DIM, C_BG);
    lcd.print(label);
}

void uiSetupScreen(const char* apName, const char* apPass) {
    dashboardDrawn = false;
    lcd.fillScreen(C_BG);

    lcd.fillRect(0, 0, SCREEN_W, HDR_H, C_ACCENT);
    lcd.setTextColor(C_BG, C_ACCENT);
    lcd.setTextSize(1);
    lcd.setCursor(8, 8);
    lcd.print("SETUP MODE — connect phone/laptop to WiFi below");

    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(10, 34);
    lcd.print("1. Connect to WiFi network:");
    lcd.setTextColor(C_CYAN, C_BG);
    lcd.setTextSize(2);
    lcd.setCursor(10, 48);
    lcd.print(apName);

    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(10, 76);
    lcd.print("Password:");
    lcd.setTextColor(C_CYAN, C_BG);
    lcd.setTextSize(2);
    lcd.setCursor(10, 90);
    lcd.print(apPass);

    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(10, 118);
    lcd.print("2. Open browser and visit:");
    lcd.setTextColor(C_CYAN, C_BG);
    lcd.setTextSize(2);
    lcd.setCursor(10, 132);
    lcd.print("192.168.4.1");

    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(10, 160);
    lcd.print("3. Enter WiFi + OAuth token");
}

void uiConnecting(const char* ssid, int attempt) {
    dashboardDrawn = false;
    lcd.fillScreen(C_BG);
    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(10, 60);
    lcd.print("Connecting to WiFi...");
    lcd.setTextColor(C_TEXT, C_BG);
    lcd.setTextSize(2);
    lcd.setCursor(10, 80);
    lcd.print(ssid);
    if (attempt > 0) {
        lcd.setTextColor(C_DIM, C_BG);
        lcd.setTextSize(1);
        lcd.setCursor(10, 115);
        lcd.printf("Attempt %d", attempt);
    }
}

void uiDashboard(const UsageData& data, unsigned long lastFetchMs, int rssi) {
    if (!dashboardDrawn || !data.ok != !lastOk) {
        drawDashboardChrome();
        lastH5 = -1; lastD7 = -1;
        lastH5Reset = 0; lastD7Reset = 0;
        lastOk = data.ok;
    }

    if (!data.ok) {
        lcd.fillRect(0, HDR_H + 1, SCREEN_W, FOOTER_Y - HDR_H - 1, C_BG);
        lcd.setTextColor(C_CRIT, C_BG);
        lcd.setTextSize(2);
        lcd.setCursor(10, 50);
        lcd.print("ERROR");
        lcd.setTextColor(C_DIM, C_BG);
        lcd.setTextSize(1);
        lcd.setCursor(10, 80);
        lcd.print(data.error);
        lcd.setCursor(10, 100);
        lcd.print("Press BTN to retry");
        return;
    }

    int barW = SCREEN_W - 24;

    // ── Bars + pct labels ─────────────────────────────
    if (data.h5 != lastH5) {
        updateBar(12, 40, barW, BAR_H, data.h5, barColor(data.h5));
        char ps[8]; snprintf(ps, sizeof(ps), "%.0f%%", data.h5);
        lcd.fillRect(SCREEN_W - 12 - 5*6, 30, 5*6, 8, C_BG);
        lcd.setTextColor(barColor(data.h5), C_BG);
        lcd.setTextSize(1);
        lcd.setCursor(SCREEN_W - 12 - (int)strlen(ps)*6, 30);
        lcd.print(ps);
        lastH5 = data.h5;
    }

    if (data.d7 != lastD7) {
        updateBar(12, 80, barW, BAR_H, data.d7, barColor(data.d7));
        char ps[8]; snprintf(ps, sizeof(ps), "%.0f%%", data.d7);
        lcd.fillRect(SCREEN_W - 12 - 5*6, 70, 5*6, 8, C_BG);
        lcd.setTextColor(barColor(data.d7), C_BG);
        lcd.setTextSize(1);
        lcd.setCursor(SCREEN_W - 12 - (int)strlen(ps)*6, 70);
        lcd.print(ps);
        lastD7 = data.d7;
    }

    // ── Countdown timers ──────────────────────────────
    char h5rst[16], d7rst[16];
    fmtCountdown(data.h5ResetEpoch, h5rst, sizeof(h5rst));
    fmtCountdown(data.d7ResetEpoch, d7rst, sizeof(d7rst));
    updateText(12,              124, 130, 18, h5rst, C_TEXT, 2);
    updateText(SCREEN_W/2 + 12, 124, 130, 18, d7rst, C_TEXT, 2);

    // ── Big percentage numbers ─────────────────────────
    char pct5[8]; snprintf(pct5, sizeof(pct5), "%.0f%%", data.h5);
    updateText(12,              172, 130, 28, pct5, barColor(data.h5), 3);
    char pct7[8]; snprintf(pct7, sizeof(pct7), "%.0f%%", data.d7);
    updateText(SCREEN_W/2 + 12, 172, 130, 28, pct7, barColor(data.d7), 3);

    // ── Header right: rssi + age ──────────────────────
    char hdr[32];
    unsigned long ago = (millis() - lastFetchMs) / 1000;
    snprintf(hdr, sizeof(hdr), "%ddBm %lus", rssi, ago);
    int hdrX = SCREEN_W - (int)strlen(hdr) * 6 - 6;
    lcd.fillRect(hdrX - 2, 2, SCREEN_W - hdrX, HDR_H - 4, C_HEAD);
    lcd.setTextColor(C_BG, C_HEAD);
    lcd.setTextSize(1);
    lcd.setCursor(hdrX, 6);
    lcd.print(hdr);
}

void uiError(const char* title, const char* detail) {
    dashboardDrawn = false;
    lcd.fillScreen(C_BG);
    lcd.setTextColor(C_CRIT, C_BG);
    lcd.setTextSize(2);
    lcd.setCursor(10, 40);
    lcd.print(title);
    if (detail) {
        lcd.setTextColor(C_DIM, C_BG);
        lcd.setTextSize(1);
        lcd.setCursor(10, 80);
        lcd.print(detail);
    }
}
