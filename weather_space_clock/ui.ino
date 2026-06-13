// ═══════════════════════════════════════════════════════
//  ui.ino — 5 pages, touch-nav bottom bar, vibrant themes
//
//  Page area: y = 0..217  (218 px)
//  Nav bar:   y = 218..239 (22 px) — split lower-left / lower-right zones
// ═══════════════════════════════════════════════════════

#include <math.h>

// Theme struct lives in CYD_ClaudeUsage.ino.

// ── Weather-driven themes (page 0) ────────────────────
static Theme themeForWeather(int code, bool isDay) {
    Theme t;
    t.text = 0xFFFF;
    t.dim  = 0x738E;
    if (code == 0 || code == 1) {
        if (isDay) { t.bg=0x0084; t.accent=0xFEA0; t.secondary=0x07FF; t.headerBg=0x0146; t.headerFg=0xFEA0; }
        else       { t.bg=0x0009; t.accent=0xC618; t.secondary=0x041F; t.headerBg=0x0010; t.headerFg=0xC618; }
        return t;
    }
    if (code >= 2 && code <= 3) {
        if (isDay) { t.bg=0x10A4; t.accent=0xFEC0; t.secondary=0xBDF7; t.headerBg=0x1965; t.headerFg=0xFEC0; }
        else       { t.bg=0x0009; t.accent=0xC618; t.secondary=0x528A; t.headerBg=0x0010; t.headerFg=0xC618; }
        return t;
    }
    if (code == 45 || code == 48) {
        t.bg=0x1082; t.accent=0xBDF7; t.secondary=0x738E; t.headerBg=0x18C3; t.headerFg=0xBDF7; return t;
    }
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
        t.bg=0x0010; t.accent=0x07FF; t.secondary=0x041F; t.headerBg=0x0017; t.headerFg=0x07FF; return t;
    }
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
        t.bg=0x0009; t.accent=0xFFFF; t.secondary=0xAEDB; t.headerBg=0x0011; t.headerFg=0xAEDB; return t;
    }
    if (code >= 95 && code <= 99) {
        t.bg=0x1003; t.accent=0xFFE0; t.secondary=0xA01F; t.headerBg=0x1804; t.headerFg=0xFFE0; return t;
    }
    t.bg=0x1082; t.accent=0xBDF7; t.secondary=0x738E; t.headerBg=0x18C3; t.headerFg=0xBDF7; return t;
}

// ── Fixed themes for space pages ─────────────────────
static Theme themeSpace() {
    return { 0x0009, 0x07FF, 0xC618, 0xFFFF, 0x738E, 0x0010, 0x07FF };
}
static Theme themeSpaceWx(float kp) {
    Theme t = themeSpace();
    // Kp-driven accent: green<3, yellow 3-5, orange 5-7, red 7+
    if      (kp >= 7) { t.accent = 0xF800; t.headerBg = 0x4000; t.headerFg = 0xFFFF; }
    else if (kp >= 5) { t.accent = 0xFD00; t.headerBg = 0x4980; t.headerFg = 0xFFFF; }
    else if (kp >= 3) { t.accent = 0xFFE0; t.headerBg = 0x4960; t.headerFg = 0x0000; }
    else              { t.accent = 0x07E0; t.headerBg = 0x0260; t.headerFg = 0xFFFF; }
    return t;
}

// ── State ────────────────────────────────────────────
static Theme theme;
static int   lastPage      = -1;
static int   lastCode      = -999;
static bool  lastIsDay     = true;
static bool  chromeDrawn   = false;

// Per-page caches (each page tracks what's already on screen)
static char  lastHHMM[8]   = "";
static char  lastAmPm[4]   = "";
static char  lastDate[24]  = "";
static char  lastCond[24]  = "";
static char  lastTemp[8]   = "";
static char  lastFeels[16] = "";
static char  lastHumid[24] = "";
static char  lastHiLo[24]  = "";
static char  lastSunR[16]  = "";
static char  lastSunS[16]  = "";
static char  lastTmrwL[40] = "";
static int   lastRssi      = 999;
static bool  lastColonOn   = true;
static int   lastFrame     = -1;
static int   lastNavPage   = -1;

// Generic dirty fields used across pages
static char  lastF1[64] = "";
static char  lastF2[64] = "";
static char  lastF3[64] = "";
static char  lastF4[64] = "";
static char  lastF5[64] = "";
static char  lastF6[64] = "";

static void resetAllCaches() {
    lastHHMM[0] = lastAmPm[0] = lastDate[0] = 0;
    lastCond[0] = lastTemp[0] = lastFeels[0] = lastHumid[0] = 0;
    lastHiLo[0] = lastSunR[0] = lastSunS[0] = lastTmrwL[0] = 0;
    lastF1[0] = lastF2[0] = lastF3[0] = lastF4[0] = lastF5[0] = lastF6[0] = 0;
    lastRssi  = 999;
    lastColonOn = true;
    lastFrame = -1;
    lastNavPage = -1;
}

// ── Drawing primitives ───────────────────────────────
static void drawText(int x, int y, int clearW, int clearH,
                     const char* s, uint16_t fg, uint8_t sz, uint16_t bg) {
    lcd.fillRect(x, y, clearW, clearH, bg);
    lcd.setTextColor(fg, bg);
    lcd.setTextSize(sz);
    lcd.setCursor(x, y);
    lcd.print(s);
}

// ── Weather icon primitives (page 0) ─────────────────
static void drawCloudShape(int cx, int cy, uint16_t color) {
    lcd.fillCircle(cx - 10, cy + 3, 7, color);
    lcd.fillCircle(cx,      cy - 5, 10, color);
    lcd.fillCircle(cx + 10, cy + 1, 8, color);
    lcd.fillRect(cx - 14, cy + 2, 28, 9, color);
}
static void drawSun(int x, int y, int frame) {
    int cx = x + 24, cy = y + 24;
    float rot = (frame / 2.0f) * ((float)PI / 8);
    for (int i = 0; i < 8; i++) {
        float a = rot + i * ((float)PI / 4);
        lcd.drawLine(cx + (int)(cosf(a) * 13), cy + (int)(sinf(a) * 13),
                     cx + (int)(cosf(a) * 20), cy + (int)(sinf(a) * 20), theme.accent);
    }
    lcd.fillCircle(cx, cy, 9, theme.accent);
}
static void drawMoonIcon(int x, int y) {
    int cx = x + 24, cy = y + 24;
    lcd.fillCircle(cx, cy, 16, theme.accent);
    lcd.fillCircle(cx + 6, cy - 4, 14, theme.bg);
}
static void drawCloud(int x, int y) { drawCloudShape(x + 24, y + 24, theme.accent); }
static void drawSunCloud(int x, int y, int frame) {
    int scx = x + 14, scy = y + 14;
    float rot = (frame / 2.0f) * ((float)PI / 8);
    for (int i = 0; i < 8; i++) {
        float a = rot + i * ((float)PI / 4);
        lcd.drawLine(scx + (int)(cosf(a) * 8),  scy + (int)(sinf(a) * 8),
                     scx + (int)(cosf(a) * 13), scy + (int)(sinf(a) * 13), theme.accent);
    }
    lcd.fillCircle(scx, scy, 6, theme.accent);
    drawCloudShape(x + 30, y + 32, theme.secondary);
}
static void drawRain(int x, int y, int frame) {
    drawCloudShape(x + 24, y + 16, theme.secondary);
    int phase = frame % 3;
    for (int i = 0; i < 5; i++) {
        int dx = x + 6 + i * 8;
        int dy = y + 28 + ((phase * 4 + i * 3) % 14);
        lcd.drawLine(dx, dy, dx - 2, dy + 6, theme.accent);
    }
}
static void drawSnow(int x, int y, int frame) {
    drawCloudShape(x + 24, y + 16, theme.secondary);
    int phase = frame % 4;
    for (int i = 0; i < 5; i++) {
        int dx = x + 6 + i * 8;
        int dy = y + 28 + ((phase + i * 2) * 3) % 16;
        lcd.drawFastHLine(dx - 2, dy,     5, theme.accent);
        lcd.drawFastVLine(dx,     dy - 2, 5, theme.accent);
    }
}
static void drawStorm(int x, int y, int frame) {
    drawCloudShape(x + 24, y + 16, theme.secondary);
    if ((frame % 4) == 0) {
        int bx = x + 22, by = y + 28;
        lcd.fillTriangle(bx,     by,     bx + 8, by,      bx + 2, by + 7,  theme.accent);
        lcd.fillTriangle(bx + 2, by + 7, bx + 10, by + 7, bx + 4, by + 16, theme.accent);
    }
}
static void drawFog(int x, int y, int frame) {
    int phase = frame % 8;
    for (int i = 0; i < 5; i++) {
        int yy = y + 10 + i * 7;
        int offset = ((phase + i) % 4) * 3;
        lcd.fillRect(x + 4 + offset, yy, 40 - offset, 3, theme.accent);
    }
}
static void drawWeatherIcon(int x, int y, int code, bool isDay, int frame) {
    lcd.fillRect(x, y, 48, 48, theme.bg);
    if (code == 0 || code == 1) { if (isDay) drawSun(x, y, frame); else drawMoonIcon(x, y); return; }
    if (code == 2 || code == 3) { if (isDay) drawSunCloud(x, y, frame); else drawCloud(x, y); return; }
    if (code == 45 || code == 48) { drawFog(x, y, frame); return; }
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) { drawRain(x, y, frame); return; }
    if ((code >= 71 && code <= 77) || code == 85 || code == 86)   { drawSnow(x, y, frame); return; }
    if (code >= 95 && code <= 99) { drawStorm(x, y, frame); return; }
    lcd.setTextColor(theme.dim, theme.bg);
    lcd.setTextSize(5);
    lcd.setCursor(x + 14, y + 8);
    lcd.print("?");
}

// ── Moon glyph (page 3) ──────────────────────────────
static void drawMoonGlyph(int cx, int cy, int r, float age,
                          uint16_t litColor, uint16_t darkColor) {
    float phase = age / 29.530588853f;
    bool  waxing = (phase < 0.5f);
    float k = 0.5f * (1.0f - cosf(2.0f * (float)PI * phase));   // illumination 0..1

    // Full lit disk, then overlay dark mask offset toward the dark side
    lcd.fillCircle(cx, cy, r, litColor);
    int dx = (int)(2.0f * r * k);
    if (waxing) lcd.fillCircle(cx - dx, cy, r, darkColor);
    else        lcd.fillCircle(cx + dx, cy, r, darkColor);
    lcd.drawCircle(cx, cy, r, 0x4208);
}

// ── Boot/error screens (use whatever theme is current) ──
void uiInit() {
    lcd.setRotation(SCREEN_ROT);
    lcd.setTextWrap(false);   // never auto-wrap long strings — we truncate instead
    theme = themeForWeather(0, true);
    lcd.fillScreen(theme.bg);
    chromeDrawn = false;
    resetAllCaches();
}

// Copy at most `maxChars` characters from src to dst, NUL-terminated.
// If truncated, replaces the final char with '>' as a visual marker.
static void clampStr(char* dst, size_t dstSize, const char* src, size_t maxChars) {
    if (dstSize == 0) return;
    size_t srcLen = strlen(src);
    size_t copy = (srcLen < maxChars) ? srcLen : maxChars;
    if (copy > dstSize - 1) copy = dstSize - 1;
    memcpy(dst, src, copy);
    dst[copy] = 0;
    if (srcLen > maxChars && copy >= 1) dst[copy - 1] = '>';
}
void uiBootProgress(int percent, const char* label) {
    chromeDrawn = false;
    lcd.fillScreen(theme.bg);
    lcd.setTextColor(theme.accent, theme.bg);
    lcd.setTextSize(2);
    lcd.setCursor(40, 40);
    lcd.print("Clock + Sky + Space");
    int bx = 30, by = 100, bw = SCREEN_W - 60, bh = 16;
    lcd.fillRect(bx, by, bw, bh, theme.headerBg);
    int fill = constrain((int)(bw * percent / 100.0f), 0, bw);
    lcd.fillRect(bx, by, fill, bh, theme.accent);
    lcd.setCursor(30, 140);
    lcd.setTextColor(theme.dim, theme.bg);
    lcd.setTextSize(1);
    lcd.print(label);
}
void uiConnecting(const char* ssid, int attempt) {
    chromeDrawn = false;
    lcd.fillScreen(theme.bg);
    lcd.setTextColor(theme.dim, theme.bg);
    lcd.setTextSize(1);
    lcd.setCursor(10, 80);
    lcd.print("Connecting to WiFi...");
    lcd.setTextColor(theme.text, theme.bg);
    lcd.setTextSize(2);
    lcd.setCursor(10, 100);
    lcd.print(ssid);
    if (attempt > 0) {
        lcd.setTextColor(theme.dim, theme.bg);
        lcd.setTextSize(1);
        lcd.setCursor(10, 130);
        lcd.printf("Attempt %d", attempt);
    }
}
void uiError(const char* title, const char* detail) {
    chromeDrawn = false;
    lcd.fillScreen(theme.bg);
    lcd.setTextColor(0xF800, theme.bg);
    lcd.setTextSize(2);
    lcd.setCursor(10, 80);
    lcd.print(title);
    if (detail) {
        lcd.setTextColor(theme.dim, theme.bg);
        lcd.setTextSize(1);
        lcd.setCursor(10, 120);
        lcd.print(detail);
    }
}

// ── Nav bar (bottom 22 px) ───────────────────────────
static const char* PAGE_LABELS[NUM_PAGES] = {
    "CLOCK", "ISS", "SPACE WX", "SKY", "LAUNCH"
};
static void drawNavBar(int page) {
    int y = SCREEN_H - NAV_H;
    lcd.fillRect(0, y, SCREEN_W, NAV_H, theme.headerBg);
    lcd.drawFastHLine(0, y, SCREEN_W, theme.accent);

    // Left arrow
    int ax = 12, ay = y + NAV_H / 2;
    lcd.fillTriangle(ax + 8, ay - 6, ax + 8, ay + 6, ax, ay, theme.headerFg);
    // Right arrow
    int bx = SCREEN_W - 12, by = ay;
    lcd.fillTriangle(bx - 8, by - 6, bx - 8, by + 6, bx, by, theme.headerFg);

    // Center: page label + dot indicators
    const char* label = PAGE_LABELS[page];
    int labelW = (int)strlen(label) * 6;
    int lx = (SCREEN_W - labelW) / 2;
    lcd.setTextColor(theme.headerFg, theme.headerBg);
    lcd.setTextSize(1);
    lcd.setCursor(lx, y + 4);
    lcd.print(label);

    int dotSpacing = 8;
    int dotsW = (NUM_PAGES - 1) * dotSpacing;
    int dx = (SCREEN_W - dotsW) / 2;
    for (int i = 0; i < NUM_PAGES; i++) {
        int cx = dx + i * dotSpacing;
        int cy = y + NAV_H - 5;
        if (i == page) lcd.fillCircle(cx, cy, 2, theme.headerFg);
        else           lcd.drawCircle(cx, cy, 2, theme.dim);
    }
    lastNavPage = page;
}

// ────────────────────────────────────────────────────
// PAGE 0 — CLOCK + WEATHER
// ────────────────────────────────────────────────────
static void drawClockPageChrome() {
    lcd.fillScreen(theme.bg);
    lcd.fillRect(0, 0, SCREEN_W, 20, theme.headerBg);
    lcd.fillRect(8, 96, SCREEN_W - 16, 2, theme.accent);
    lcd.fillRect(8, 152, SCREEN_W - 16, 2, theme.accent);
    chromeDrawn = true;
    resetAllCaches();
}

static void drawClockPage(const struct tm& t, const WeatherData& w, int rssi) {
    int code  = w.ok ? w.weatherCode : 0;
    bool isDay = w.ok ? w.isDay : (t.tm_hour >= 6 && t.tm_hour < 19);
    if (code != lastCode || isDay != lastIsDay) {
        theme = themeForWeather(code, isDay);
        lastCode = code; lastIsDay = isDay;
        chromeDrawn = false;
    }
    if (!chromeDrawn) drawClockPageChrome();

    // Header
    static const char* DAYS[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char* MONTHS[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    char date[24];
    snprintf(date, sizeof(date), "%s, %s %d, %d",
             DAYS[t.tm_wday], MONTHS[t.tm_mon], t.tm_mday, t.tm_year + 1900);
    if (strcmp(date, lastDate) != 0) {
        drawText(8, 6, 220, 12, date, theme.headerFg, 1, theme.headerBg);
        strlcpy(lastDate, date, sizeof(lastDate));
    }
    if (rssi != lastRssi) {
        char rs[12]; snprintf(rs, sizeof(rs), "%ddBm", rssi);
        int rw = (int)strlen(rs) * 6;
        drawText(SCREEN_W - rw - 8, 6, rw, 12, rs, theme.headerFg, 1, theme.headerBg);
        lastRssi = rssi;
    }

    // Clock
    char hhmm[8];
    int h12 = t.tm_hour % 12; if (h12 == 0) h12 = 12;
    snprintf(hhmm, sizeof(hhmm), "%d:%02d", h12, t.tm_min);
    bool hhmmChanged = strcmp(hhmm, lastHHMM) != 0;
    int  textW = (int)strlen(hhmm) * 36;
    int  startX = (SCREEN_W - textW) / 2;
    if (hhmmChanged) {
        lcd.fillRect(0, 24, SCREEN_W, 48, theme.bg);
        lcd.setTextColor(theme.text, theme.bg);
        lcd.setTextSize(6);
        lcd.setCursor(startX, 24);
        lcd.print(hhmm);
        strlcpy(lastHHMM, hhmm, sizeof(lastHHMM));
        lastColonOn = true;
    }
    bool colonOn = (t.tm_sec % 2) == 0;
    if (colonOn != lastColonOn) {
        const char* colonPtr = strchr(hhmm, ':');
        int colonIdx = colonPtr - hhmm;
        int colonX = startX + colonIdx * 36;
        lcd.fillRect(colonX, 24, 36, 48, theme.bg);
        lcd.setTextColor(colonOn ? theme.text : theme.bg, theme.bg);
        lcd.setTextSize(6);
        lcd.setCursor(colonX, 24);
        lcd.print(':');
        lastColonOn = colonOn;
    }

    // AM/PM
    const char* ampm = (t.tm_hour >= 12) ? "PM" : "AM";
    if (strcmp(ampm, lastAmPm) != 0) {
        int aw = (int)strlen(ampm) * 12;
        drawText((SCREEN_W - aw) / 2, 76, aw + 4, 16, ampm, theme.dim, 2, theme.bg);
        strlcpy(lastAmPm, ampm, sizeof(lastAmPm));
    }

    // Weather block
    if (w.ok) {
        int frame = (int)(millis() / 1000);
        if (frame != lastFrame) {
            drawWeatherIcon(8, 102, w.weatherCode, w.isDay, frame);
            lastFrame = frame;
        }
        char temp[8];
        if (isnan(w.tempF)) strlcpy(temp, "--", sizeof(temp));
        else                snprintf(temp, sizeof(temp), "%.0f", w.tempF);
        if (strcmp(temp, lastTemp) != 0) {
            // Clear width 88 ends at x=152 so the wipe never reaches the
            // right-column text that starts at x=156. (Was 110, which clobbered
            // the first 18 px of "Overcast" / "RH …" on every temp refresh.)
            drawText(64, 102, 88, 44, "", theme.bg, 1, theme.bg);
            lcd.setTextColor(theme.accent, theme.bg);
            lcd.setTextSize(5);
            lcd.setCursor(64, 102);
            lcd.print(temp);
            int afterX = 64 + (int)strlen(temp) * 30 + 2;
            lcd.setTextSize(2);
            lcd.setTextColor(theme.dim, theme.bg);
            lcd.setCursor(afterX, 102);
            lcd.print((char)247); lcd.print('F');
            strlcpy(lastTemp, temp, sizeof(lastTemp));
            // Belt + suspenders: force the right-column text to redraw this
            // tick in case anything overlapped during the clear/redraw.
            lastCond[0]  = 0;
            lastFeels[0] = 0;
            lastHumid[0] = 0;
        }
        const char* cond = weatherCodeShort(w.weatherCode);
        if (strcmp(cond, lastCond) != 0) {
            // Width 160 at size 2 fits up to 13 chars (longest label "T-storm/Hail")
            drawText(156, 104, SCREEN_W - 156 - 4, 16, cond, theme.secondary, 2, theme.bg);
            strlcpy(lastCond, cond, sizeof(lastCond));
        }
        char feels[16];
        if (isnan(w.feelsF)) strlcpy(feels, "Feels --", sizeof(feels));
        else                 snprintf(feels, sizeof(feels), "Feels %.0f%c", w.feelsF, 247);
        if (strcmp(feels, lastFeels) != 0) {
            drawText(156, 124, 160, 10, feels, theme.dim, 1, theme.bg);
            strlcpy(lastFeels, feels, sizeof(lastFeels));
        }
        char humid[24];
        if (isnan(w.windMph)) snprintf(humid, sizeof(humid), "RH %d%%", w.humidity);
        else                  snprintf(humid, sizeof(humid), "RH %d%%  Wind %.0f mph", w.humidity, w.windMph);
        if (strcmp(humid, lastHumid) != 0) {
            drawText(156, 138, 160, 10, humid, theme.dim, 1, theme.bg);
            strlcpy(lastHumid, humid, sizeof(lastHumid));
        }

        // Bottom — Hi/Lo, sun, tomorrow
        char hilo[24];
        if (isnan(w.todayHi) || isnan(w.todayLo)) strlcpy(hilo, "H -- / L --", sizeof(hilo));
        else snprintf(hilo, sizeof(hilo), "H %.0f%c  L %.0f%c", w.todayHi, 247, w.todayLo, 247);
        if (strcmp(hilo, lastHiLo) != 0) {
            drawText(8, 160, 170, 16, hilo, theme.text, 2, theme.bg);
            strlcpy(lastHiLo, hilo, sizeof(lastHiLo));
        }
        char sr[20]; snprintf(sr, sizeof(sr), "Sunrise  %s", w.sunrise);
        if (strcmp(sr, lastSunR) != 0) {
            drawText(8, 182, 170, 10, sr, theme.dim, 1, theme.bg);
            strlcpy(lastSunR, sr, sizeof(lastSunR));
        }
        char ss[20]; snprintf(ss, sizeof(ss), "Sunset   %s", w.sunset);
        if (strcmp(ss, lastSunS) != 0) {
            drawText(8, 196, 170, 10, ss, theme.dim, 1, theme.bg);
            strlcpy(lastSunS, ss, sizeof(lastSunS));
        }
        char tmrwLine[40];
        snprintf(tmrwLine, sizeof(tmrwLine), "%s|%.0f|%.0f", weatherCodeShort(w.tmrwCode),
                 isnan(w.tmrwHi)?0:w.tmrwHi, isnan(w.tmrwLo)?0:w.tmrwLo);
        if (strcmp(tmrwLine, lastTmrwL) != 0) {
            drawText(190, 160, 130, 50, "", theme.bg, 1, theme.bg);
            lcd.setTextColor(theme.dim, theme.bg);
            lcd.setTextSize(1);
            lcd.setCursor(190, 160); lcd.print("Tomorrow");
            lcd.setTextColor(theme.secondary, theme.bg);
            lcd.setTextSize(2);
            lcd.setCursor(190, 174); lcd.print(weatherCodeShort(w.tmrwCode));
            lcd.setTextColor(theme.text, theme.bg);
            lcd.setTextSize(1);
            char hl[20];
            if (isnan(w.tmrwHi) || isnan(w.tmrwLo)) strlcpy(hl, "-- / --", sizeof(hl));
            else snprintf(hl, sizeof(hl), "H %.0f%c  L %.0f%c", w.tmrwHi, 247, w.tmrwLo, 247);
            lcd.setCursor(190, 196); lcd.print(hl);
            strlcpy(lastTmrwL, tmrwLine, sizeof(lastTmrwL));
        }
    } else {
        const char* msg = "(loading weather...)";
        if (strcmp(msg, lastCond) != 0) {
            drawText(8, 110, SCREEN_W - 16, 18, msg, theme.dim, 2, theme.bg);
            strlcpy(lastCond, msg, sizeof(lastCond));
        }
    }
}

// ────────────────────────────────────────────────────
// PAGE 1 — ISS LIVE
// ────────────────────────────────────────────────────
static void drawIssChrome() {
    lcd.fillScreen(theme.bg);
    lcd.fillRect(0, 0, SCREEN_W, 20, theme.headerBg);
    lcd.setTextColor(theme.headerFg, theme.headerBg);
    lcd.setTextSize(2);
    lcd.setCursor(8, 3);
    lcd.print("ISS LIVE");
    chromeDrawn = true;
    resetAllCaches();
}
static void drawIssPage(const ISSData& iss, int rssi) {
    if (!chromeDrawn) drawIssChrome();
    if (rssi != lastRssi) {
        char rs[12]; snprintf(rs, sizeof(rs), "%ddBm", rssi);
        int rw = (int)strlen(rs) * 6;
        drawText(SCREEN_W - rw - 8, 6, rw, 12, rs, theme.headerFg, 1, theme.headerBg);
        lastRssi = rssi;
    }
    if (!iss.ok) {
        const char* msg = "(no ISS data)";
        if (strcmp(msg, lastF1) != 0) {
            drawText(10, 90, SCREEN_W - 20, 20, msg, theme.dim, 2, theme.bg);
            strlcpy(lastF1, msg, sizeof(lastF1));
        }
        return;
    }

    // Big lat/lon
    char latlon[40];
    snprintf(latlon, sizeof(latlon), "%.2f, %.2f", iss.lat, iss.lon);
    if (strcmp(latlon, lastF1) != 0) {
        drawText(8, 30, SCREEN_W - 16, 32, "", theme.bg, 1, theme.bg);
        lcd.setTextColor(theme.accent, theme.bg);
        lcd.setTextSize(3);
        lcd.setCursor(8, 32);
        lcd.print(latlon);
        strlcpy(lastF1, latlon, sizeof(lastF1));
    }
    // Label under lat/lon
    if (lastF2[0] == 0) {
        drawText(8, 66, 200, 10, "latitude, longitude", theme.dim, 1, theme.bg);
        strlcpy(lastF2, "ll", sizeof(lastF2));
    }

    // Altitude + Velocity
    char alt[24];  snprintf(alt, sizeof(alt), "%.0f km",  iss.altKm);
    char vel[24];  snprintf(vel, sizeof(vel), "%.0f km/h", iss.velKmH);
    if (strcmp(alt, lastF3) != 0) {
        drawText(8,  84, 150, 24, "", theme.bg, 1, theme.bg);
        lcd.setTextColor(theme.dim, theme.bg); lcd.setTextSize(1);
        lcd.setCursor(8, 84); lcd.print("ALTITUDE");
        lcd.setTextColor(theme.text, theme.bg); lcd.setTextSize(2);
        lcd.setCursor(8, 96); lcd.print(alt);
        strlcpy(lastF3, alt, sizeof(lastF3));
    }
    if (strcmp(vel, lastF4) != 0) {
        drawText(170, 84, 145, 24, "", theme.bg, 1, theme.bg);
        lcd.setTextColor(theme.dim, theme.bg); lcd.setTextSize(1);
        lcd.setCursor(170, 84); lcd.print("VELOCITY");
        lcd.setTextColor(theme.text, theme.bg); lcd.setTextSize(2);
        lcd.setCursor(170, 96); lcd.print(vel);
        strlcpy(lastF4, vel, sizeof(lastF4));
    }

    // Distance from user + visibility
    char dist[32]; snprintf(dist, sizeof(dist), "%.0f km from you", iss.distFromUserKm);
    if (strcmp(dist, lastF5) != 0) {
        drawText(8, 130, SCREEN_W - 16, 20, dist, theme.secondary, 2, theme.bg);
        strlcpy(lastF5, dist, sizeof(lastF5));
    }

    char people[40];
    if (iss.peopleInSpace >= 0)
        snprintf(people, sizeof(people), "%d in space  |  ISS %s",
                 iss.peopleInSpace, iss.visibility);
    else
        snprintf(people, sizeof(people), "ISS %s", iss.visibility);
    if (strcmp(people, lastF6) != 0) {
        drawText(8, 160, SCREEN_W - 16, 14, people, theme.dim, 1, theme.bg);
        strlcpy(lastF6, people, sizeof(lastF6));
    }
}

// ────────────────────────────────────────────────────
// PAGE 2 — SPACE WEATHER
// ────────────────────────────────────────────────────
static const char* kpLevel(float kp) {
    if (kp >= 7) return "STORM";
    if (kp >= 5) return "AURORA";
    if (kp >= 3) return "ACTIVE";
    return "QUIET";
}
static void drawSpaceWxChrome() {
    lcd.fillScreen(theme.bg);
    lcd.fillRect(0, 0, SCREEN_W, 20, theme.headerBg);
    lcd.setTextColor(theme.headerFg, theme.headerBg);
    lcd.setTextSize(2);
    lcd.setCursor(8, 3);
    lcd.print("SPACE WX");
    chromeDrawn = true;
    resetAllCaches();
}
static void drawSpaceWxPage(const SpaceWxData& w, int rssi) {
    Theme nt = themeSpaceWx(w.ok ? w.kp : 0);
    if (memcmp(&nt, &theme, sizeof(Theme)) != 0) {
        theme = nt; chromeDrawn = false;
    }
    if (!chromeDrawn) drawSpaceWxChrome();
    if (rssi != lastRssi) {
        char rs[12]; snprintf(rs, sizeof(rs), "%ddBm", rssi);
        int rw = (int)strlen(rs) * 6;
        drawText(SCREEN_W - rw - 8, 6, rw, 12, rs, theme.headerFg, 1, theme.headerBg);
        lastRssi = rssi;
    }
    if (!w.ok) {
        const char* msg = "(no data)";
        if (strcmp(msg, lastF1) != 0) {
            drawText(10, 90, 200, 20, msg, theme.dim, 2, theme.bg);
            strlcpy(lastF1, msg, sizeof(lastF1));
        }
        return;
    }

    // Big Kp
    char kp[8]; snprintf(kp, sizeof(kp), "%.1f", w.kp);
    if (strcmp(kp, lastF1) != 0) {
        drawText(8, 32, 130, 56, "", theme.bg, 1, theme.bg);
        lcd.setTextColor(theme.dim, theme.bg);
        lcd.setTextSize(1);
        lcd.setCursor(8, 32); lcd.print("Kp INDEX");
        lcd.setTextColor(theme.accent, theme.bg);
        lcd.setTextSize(7);
        lcd.setCursor(8, 44); lcd.print(kp);
        strlcpy(lastF1, kp, sizeof(lastF1));
    }
    const char* level = kpLevel(w.kp);
    if (strcmp(level, lastF2) != 0) {
        drawText(150, 46, 160, 24, "", theme.bg, 1, theme.bg);
        lcd.setTextColor(theme.accent, theme.bg);
        lcd.setTextSize(3);
        lcd.setCursor(150, 46);
        lcd.print(level);
        strlcpy(lastF2, level, sizeof(lastF2));
    }

    // Aurora indicator: a 9-step bar
    static int lastBar = -1;
    int barFill = constrain((int)w.kp, 0, 9);
    if (barFill != lastBar) {
        int bx = 8, by = 112, bw = SCREEN_W - 16, bh = 12;
        lcd.fillRect(bx, by, bw, bh, theme.headerBg);
        for (int i = 0; i < 9; i++) {
            int segW = (bw - 8) / 9;
            int sx = bx + 4 + i * segW;
            uint16_t segCol = theme.dim;
            if (i < barFill) {
                if (i >= 7) segCol = 0xF800;
                else if (i >= 5) segCol = 0xFD00;
                else if (i >= 3) segCol = 0xFFE0;
                else segCol = 0x07E0;
            }
            lcd.fillRect(sx, by + 2, segW - 2, bh - 4, segCol);
        }
        lastBar = barFill;
    }

    // Solar wind + flare
    char sw[24]; snprintf(sw, sizeof(sw), "Solar wind %d km/s", w.windSpeed);
    if (strcmp(sw, lastF3) != 0) {
        drawText(8, 140, 200, 16, sw, theme.text, 2, theme.bg);
        strlcpy(lastF3, sw, sizeof(lastF3));
    }
    char fl[24]; snprintf(fl, sizeof(fl), "Latest flare %s", w.flareClass);
    if (strcmp(fl, lastF4) != 0) {
        drawText(8, 168, 250, 16, fl, theme.secondary, 2, theme.bg);
        strlcpy(lastF4, fl, sizeof(lastF4));
    }
    const char* hint;
    if (w.kp >= 5) hint = "Aurora may be visible from N. US!";
    else if (w.kp >= 3) hint = "Aurora visible at high latitudes";
    else hint = "Quiet geomagnetic conditions";
    if (strcmp(hint, lastF5) != 0) {
        drawText(8, 192, SCREEN_W - 16, 12, hint, theme.dim, 1, theme.bg);
        strlcpy(lastF5, hint, sizeof(lastF5));
    }
}

// ────────────────────────────────────────────────────
// PAGE 3 — SKY TONIGHT
// ────────────────────────────────────────────────────
static void drawSkyChrome() {
    theme = themeSpace();
    lcd.fillScreen(theme.bg);
    lcd.fillRect(0, 0, SCREEN_W, 20, theme.headerBg);
    lcd.setTextColor(theme.headerFg, theme.headerBg);
    lcd.setTextSize(2);
    lcd.setCursor(8, 3);
    lcd.print("SKY TONIGHT");
    chromeDrawn = true;
    resetAllCaches();
}
static void drawSkyPage(const SkyData& sky, const WeatherData& w, int rssi) {
    if (!chromeDrawn) drawSkyChrome();
    if (rssi != lastRssi) {
        char rs[12]; snprintf(rs, sizeof(rs), "%ddBm", rssi);
        int rw = (int)strlen(rs) * 6;
        drawText(SCREEN_W - rw - 8, 6, rw, 12, rs, theme.headerFg, 1, theme.headerBg);
        lastRssi = rssi;
    }

    // Big moon glyph on left
    char phaseKey[40];
    snprintf(phaseKey, sizeof(phaseKey), "%.2f|%s", sky.moonAge, sky.moonPhase);
    if (strcmp(phaseKey, lastF1) != 0) {
        lcd.fillRect(8, 32, 96, 96, theme.bg);
        drawMoonGlyph(56, 80, 44, sky.moonAge, 0xEF7D, theme.bg);
        strlcpy(lastF1, phaseKey, sizeof(lastF1));
    }

    // Right: phase name + illumination + age
    if (strcmp(sky.moonPhase, lastF2) != 0) {
        drawText(116, 38, SCREEN_W - 116 - 4, 18, sky.moonPhase, theme.accent, 2, theme.bg);
        strlcpy(lastF2, sky.moonPhase, sizeof(lastF2));
    }
    char illum[24]; snprintf(illum, sizeof(illum), "%.0f%% illuminated", sky.moonIllum);
    if (strcmp(illum, lastF3) != 0) {
        drawText(116, 64, SCREEN_W - 116 - 4, 10, illum, theme.dim, 1, theme.bg);
        strlcpy(lastF3, illum, sizeof(lastF3));
    }
    char age[24]; snprintf(age, sizeof(age), "Age %.1f days", sky.moonAge);
    if (strcmp(age, lastF4) != 0) {
        drawText(116, 80, SCREEN_W - 116 - 4, 10, age, theme.dim, 1, theme.bg);
        strlcpy(lastF4, age, sizeof(lastF4));
    }

    // Sun rise/set at the bottom (from weather data)
    if (w.ok) {
        char sr[20]; snprintf(sr, sizeof(sr), "Sunrise %s", w.sunrise);
        if (strcmp(sr, lastF5) != 0) {
            drawText(8, 156, 180, 16, sr, theme.text, 2, theme.bg);
            strlcpy(lastF5, sr, sizeof(lastF5));
        }
        char ss[20]; snprintf(ss, sizeof(ss), "Sunset  %s", w.sunset);
        if (strcmp(ss, lastF6) != 0) {
            drawText(8, 180, 180, 16, ss, theme.text, 2, theme.bg);
            strlcpy(lastF6, ss, sizeof(lastF6));
        }
    }
}

// ────────────────────────────────────────────────────
// PAGE 4 — NEXT LAUNCH
// ────────────────────────────────────────────────────
static void drawLaunchChrome() {
    theme = themeSpace();
    lcd.fillScreen(theme.bg);
    lcd.fillRect(0, 0, SCREEN_W, 20, theme.headerBg);
    lcd.setTextColor(theme.headerFg, theme.headerBg);
    lcd.setTextSize(2);
    lcd.setCursor(8, 3);
    lcd.print("NEXT LAUNCH");
    chromeDrawn = true;
    resetAllCaches();
}
static void fmtCountdown(time_t deltaSec, char* out, size_t len) {
    if (deltaSec < 0) {
        long lifted = -deltaSec;
        long h = lifted / 3600;
        long m = (lifted % 3600) / 60;
        long s = lifted % 60;
        snprintf(out, len, "T+%02ld:%02ld:%02ld", h, m, s);
        return;
    }
    long d = deltaSec / 86400;
    long h = (deltaSec % 86400) / 3600;
    long m = (deltaSec % 3600) / 60;
    long s = deltaSec % 60;
    if (d > 0) snprintf(out, len, "T-%ldd %02ld:%02ld:%02ld", d, h, m, s);
    else       snprintf(out, len, "T-%02ld:%02ld:%02ld", h, m, s);
}
static void drawLaunchPage(const LaunchData& l, int rssi) {
    if (!chromeDrawn) drawLaunchChrome();
    if (rssi != lastRssi) {
        char rs[12]; snprintf(rs, sizeof(rs), "%ddBm", rssi);
        int rw = (int)strlen(rs) * 6;
        drawText(SCREEN_W - rw - 8, 6, rw, 12, rs, theme.headerFg, 1, theme.headerBg);
        lastRssi = rssi;
    }
    if (!l.ok) {
        const char* msg = "(no launch data)";
        if (strcmp(msg, lastF1) != 0) {
            drawText(10, 90, 300, 20, msg, theme.dim, 2, theme.bg);
            strlcpy(lastF1, msg, sizeof(lastF1));
        }
        return;
    }

    // Provider — size 2, max 25 chars to fit 320 px width
    char prov[28]; clampStr(prov, sizeof(prov), l.provider, 25);
    if (strcmp(prov, lastF1) != 0) {
        drawText(8, 28, SCREEN_W - 16, 18, prov, theme.accent, 2, theme.bg);
        strlcpy(lastF1, prov, sizeof(lastF1));
    }
    // Vehicle — size 2, separate line, max 25 chars
    char veh[28]; clampStr(veh, sizeof(veh), l.vehicle, 25);
    if (strcmp(veh, lastF2) != 0) {
        drawText(8, 50, SCREEN_W - 16, 18, veh, theme.secondary, 2, theme.bg);
        strlcpy(lastF2, veh, sizeof(lastF2));
    }
    // Mission name — size 1, fits ~50 chars
    char miss[52]; clampStr(miss, sizeof(miss), l.name, 50);
    if (strcmp(miss, lastF6) != 0) {
        drawText(8, 72, SCREEN_W - 16, 12, miss, theme.dim, 1, theme.bg);
        strlcpy(lastF6, miss, sizeof(lastF6));
    }
    // Countdown (live)
    time_t now; time(&now);
    char cd[24]; fmtCountdown(l.netEpoch - now, cd, sizeof(cd));
    if (strcmp(cd, lastF3) != 0) {
        drawText(8, 92, SCREEN_W - 16, 36, "", theme.bg, 1, theme.bg);
        lcd.setTextColor(theme.text, theme.bg);
        lcd.setTextSize(4);
        lcd.setCursor(8, 92);
        lcd.print(cd);
        strlcpy(lastF3, cd, sizeof(lastF3));
    }
    // Pad / location — size 1, truncated to ~50 chars
    char padBuf[56]; char padTrunc[52];
    clampStr(padTrunc, sizeof(padTrunc), l.pad, 46);
    snprintf(padBuf, sizeof(padBuf), "Pad: %s", padTrunc);
    if (strcmp(padBuf, lastF4) != 0) {
        drawText(8, 140, SCREEN_W - 16, 12, padBuf, theme.dim, 1, theme.bg);
        strlcpy(lastF4, padBuf, sizeof(lastF4));
    }
    // Launch time in local
    if (l.netEpoch > 0 && lastF5[0] == 0) {
        struct tm lt; localtime_r(&l.netEpoch, &lt);
        char tm[40];
        snprintf(tm, sizeof(tm), "%04d-%02d-%02d  %02d:%02d local",
                 lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min);
        drawText(8, 156, SCREEN_W - 16, 12, tm, theme.dim, 1, theme.bg);
        strlcpy(lastF5, tm, sizeof(lastF5));
    }
}

// ────────────────────────────────────────────────────
// MAIN DISPATCHER
// ────────────────────────────────────────────────────
void uiRender(int page,
              const struct tm& t,
              const WeatherData& w,
              const ISSData& iss,
              const SpaceWxData& spwx,
              const SkyData& sky,
              const LaunchData& launch,
              int rssi, bool forceRedraw) {
    if (page != lastPage || forceRedraw) {
        chromeDrawn = false;
        lastPage = page;
        lastCode = -999;   // force theme re-eval on page 0
    }

    switch (page) {
        case PAGE_CLOCK:    drawClockPage(t, w, rssi); break;
        case PAGE_ISS:      theme = themeSpace(); drawIssPage(iss, rssi); break;
        case PAGE_SPACE_WX: drawSpaceWxPage(spwx, rssi); break;
        case PAGE_SKY:      drawSkyPage(sky, w, rssi); break;
        case PAGE_LAUNCH:   drawLaunchPage(launch, rssi); break;
    }
    if (page != lastNavPage || forceRedraw) drawNavBar(page);
}
