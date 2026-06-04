/*
 * Clock + Weather + Space Display — CYD (ESP32-2432S028R)
 * ========================================================
 *   5 swipeable pages (touch-nav at bottom):
 *     0  Clock + Weather  (NTP, Open-Meteo)
 *     1  ISS Live         (wheretheiss.at + Open Notify astros)
 *     2  Space Weather    (NOAA SWPC: Kp, solar wind, X-ray flares)
 *     3  Sky Tonight      (moon phase, locally calculated)
 *     4  Next Launch      (Launch Library 2 / TheSpaceDevs)
 *
 *   Touch:
 *     Tap lower-left  → previous page
 *     Tap lower-right → next page
 *
 * FIRST-TIME TOUCH CALIBRATION:
 *   Set CALIBRATE_TOUCH to 1, upload, follow on-screen prompts,
 *   copy the 5 numbers from Serial Monitor into CAL_DATA[] below,
 *   set CALIBRATE_TOUCH back to 0, re-upload.
 *
 * LIBRARIES:
 *   - TFT_eSPI      by Bodmer
 *   - ArduinoJson   by Benoit Blanchon (v7)
 */

// ── Includes ──────────────────────────────────────────
#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <math.h>

// ── Credentials ──────────────────────────────────────
// Real values live in secrets.h (gitignored). If you cloned this repo,
// copy secrets.h.example → secrets.h and fill in your WiFi info.
#include "secrets.h"

// ── Location (Oak Ridge / Knoxville, TN) ──────────────
#define LATITUDE      35.8109
#define LONGITUDE    -84.2333
#define LOCAL_TZ      "EST5EDT,M3.2.0,M11.1.0"

// ── Touch (XPT2046 on dedicated VSPI bus — 2USB CYD) ──
// Pins per ESP32-2432S028R 2USB revision schematic
#define XPT_CS    33
#define XPT_IRQ   36
#define XPT_CLK   25
#define XPT_MISO  39
#define XPT_MOSI  32

// Raw ADC value range from XPT2046 — tune if touch zones feel off.
// Set CALIBRATE_TOUCH=1 to log raw values for taps on Serial; tap each
// corner, read min/max from the log, paste below, set the flag back to 0.
#define CALIBRATE_TOUCH 0
#define TOUCH_RAW_X_MIN  350
#define TOUCH_RAW_X_MAX 3580
#define TOUCH_RAW_Y_MIN  500
#define TOUCH_RAW_Y_MAX 3650

static SPIClass touchSPI(VSPI);
static XPT2046_Touchscreen ts(XPT_CS, XPT_IRQ);

// ── Tuning ────────────────────────────────────────────
#define BRIGHTNESS_PWM         200
#define SCREEN_W               320
#define SCREEN_H               240
#define SCREEN_ROT               1
#define NAV_H                   22       // bottom nav strip height
#define WIFI_CONNECT_TIMEOUT_S  30
#define API_TIMEOUT_MS       12000

// Refresh intervals (seconds)
#define WEATHER_REFRESH_SEC    900       // 15 min
#define ISS_REFRESH_SEC         30       // (was 8) less main-loop blocking
#define ASTROS_REFRESH_SEC    3600       // 1 hr
#define SPACEWX_REFRESH_SEC   3600       // (was 900) hourly is plenty
#define LAUNCH_REFRESH_SEC    3600       // (was 1800) hourly is plenty

// ── Shared structs ────────────────────────────────────
// All structs that cross .ino files MUST be defined here so that
// Arduino's auto-prototype generator sees them first.

struct WeatherData {
    bool      ok;
    float     tempF;
    float     feelsF;
    int       humidity;
    float     windMph;
    int       weatherCode;
    bool      isDay;
    float     todayHi;
    float     todayLo;
    float     tmrwHi;
    float     tmrwLo;
    int       tmrwCode;
    char      sunrise[12];
    char      sunset[12];
    char      error[40];
};

struct Theme {
    uint16_t bg;
    uint16_t accent;
    uint16_t secondary;
    uint16_t text;
    uint16_t dim;
    uint16_t headerBg;
    uint16_t headerFg;
};

struct ISSData {
    bool      ok;
    float     lat;
    float     lon;
    float     altKm;
    float     velKmH;
    int       peopleInSpace;
    float     distFromUserKm;
    char      visibility[16];
    char      error[40];
};

struct SpaceWxData {
    bool      ok;
    float     kp;             // 0-9
    int       windSpeed;       // km/s (solar wind)
    float     bz;             // nT
    char      flareClass[8];   // e.g. "M2.5"
    char      error[40];
};

struct LaunchData {
    bool      ok;
    char      name[64];
    char      provider[40];
    char      vehicle[40];
    char      pad[48];
    time_t    netEpoch;
    char      error[40];
};

struct SkyData {
    float     moonAge;          // 0..29.53 days
    float     moonIllum;        // 0..100 %
    char      moonPhase[20];    // "Waxing Crescent" etc
};

// ── Page enum ─────────────────────────────────────────
enum Page {
    PAGE_CLOCK = 0,
    PAGE_ISS,
    PAGE_SPACE_WX,
    PAGE_SKY,
    PAGE_LAUNCH,
    NUM_PAGES
};

// ── Global LCD instance ──────────────────────────────
TFT_eSPI lcd;

// ── HAL pin definitions ──────────────────────────────
#define BL_PIN     21

// ── Shared state ─────────────────────────────────────
// `weather`, `iss`, `spwx`, `launch` are written by the fetcher task on core 0
// and read by the display loop on core 1. The mutex makes the writes atomic
// from the reader's point of view: the fetcher writes into a local struct
// during the (slow) HTTP call, then briefly takes the mutex to memcpy it into
// the global. Reader takes the mutex just long enough to snapshot all four
// structs into locals, then renders without holding it.
// `sky` is calculated and read only on core 1 — no mutex needed.
static WeatherData    weather;
static ISSData        iss;
static SpaceWxData    spwx;
static LaunchData     launch;
static SkyData        sky;

static SemaphoreHandle_t dataMutex = NULL;

static unsigned long lastWeatherMs = 0;
static unsigned long lastIssMs     = 0;
static unsigned long lastAstrosMs  = 0;
static unsigned long lastSpaceWxMs = 0;
static unsigned long lastLaunchMs  = 0;

static int           currentPage   = PAGE_CLOCK;
static bool          forceRedraw   = true;
static unsigned long lastTouchMs   = 0;

// ── HAL ──────────────────────────────────────────────
static void halInit() {
    lcd.init();
    lcd.setRotation(SCREEN_ROT);
    ledcAttach(BL_PIN, 5000, 8);
    ledcWrite(BL_PIN, BRIGHTNESS_PWM);
}

// ── WiFi connect: with-UI version (core 1 only) ──────
// Draws "Connecting..." to the LCD. Safe ONLY from core 1 (the only core
// that touches TFT_eSPI).
static bool connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int ticks = 0;
    while (WiFi.status() != WL_CONNECTED) {
        ticks++;
        uiConnecting(WIFI_SSID, ticks / 2);
        delay(500);
        if (ticks > WIFI_CONNECT_TIMEOUT_S * 2) return false;
    }
    Serial.printf("[wifi] connected, IP=%s\n", WiFi.localIP().toString().c_str());
    return true;
}

// Silent reconnect helper used by the fetcher task on core 0.
// Does NOT touch TFT_eSPI (which is not multi-thread safe).
static bool connectWiFiSilent() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int ticks = 0;
    while (WiFi.status() != WL_CONNECTED) {
        ticks++;
        vTaskDelay(pdMS_TO_TICKS(500));
        if (ticks > WIFI_CONNECT_TIMEOUT_S * 2) return false;
    }
    Serial.printf("[wifi] reconnected (core %d), IP=%s\n",
                  xPortGetCoreID(), WiFi.localIP().toString().c_str());
    return true;
}

static void syncTime() {
    configTzTime(LOCAL_TZ, "pool.ntp.org", "time.nist.gov", "time.google.com");
    struct tm t;
    getLocalTime(&t, 8000);
}

// ── Per-page refresh dispatchers ─────────────────────
// Each fetcher writes to a local temp struct (the slow HTTP call); only the
// brief memcpy into the global is mutex-protected. This keeps the renderer
// on core 1 unblocked even during a 2-second HTTPS round-trip.
//
// Failure handling: a failed fetch (tmp.ok == false) does NOT publish to the
// global — the screen keeps showing the last good data. We also back off only
// 60 s on failure (vs the full refresh interval) so transient API hiccups
// recover quickly instead of locking us out for 15+ minutes.
//
// Every tmp struct is zero-initialised: a sub-fetch inside a multi-step
// fetcher (e.g. fetchSpaceWx makes three GETs) might leave some fields unset
// on partial failure. Zeroing avoids publishing uninitialised stack garbage
// like "wind=1125984367km/s".
#define FETCH_RETRY_AFTER_FAIL_SEC  60

static void maybeRefreshAll() {
    unsigned long now = millis();

    auto scheduleRetry = [&](unsigned long& lastMs, unsigned long intervalSec) {
        // Set lastMs so the next attempt happens FETCH_RETRY_AFTER_FAIL_SEC
        // from now instead of waiting the full interval.
        lastMs = now - (intervalSec * 1000UL) + (FETCH_RETRY_AFTER_FAIL_SEC * 1000UL);
    };

    if (now - lastWeatherMs >= (unsigned long)WEATHER_REFRESH_SEC * 1000UL ||
        lastWeatherMs == 0) {
        WeatherData tmp = {};
        fetchWeather(tmp);
        if (tmp.ok) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            memcpy(&weather, &tmp, sizeof(weather));
            xSemaphoreGive(dataMutex);
            lastWeatherMs = now;
        } else {
            Serial.printf("[weather] fail (%s) — retry in %ds\n",
                          tmp.error, FETCH_RETRY_AFTER_FAIL_SEC);
            scheduleRetry(lastWeatherMs, WEATHER_REFRESH_SEC);
        }
    }
    if (now - lastIssMs >= (unsigned long)ISS_REFRESH_SEC * 1000UL ||
        lastIssMs == 0) {
        // fetchISS overwrites everything except peopleInSpace; snapshot the
        // current value so we don't clobber it.
        ISSData tmp = {};
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        memcpy(&tmp, &iss, sizeof(tmp));
        xSemaphoreGive(dataMutex);
        fetchISS(tmp);
        if (tmp.ok) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            memcpy(&iss, &tmp, sizeof(iss));
            xSemaphoreGive(dataMutex);
            lastIssMs = now;
        } else {
            Serial.printf("[iss] fail (%s) — retry in %ds\n",
                          tmp.error, FETCH_RETRY_AFTER_FAIL_SEC);
            scheduleRetry(lastIssMs, ISS_REFRESH_SEC);
        }
    }
    if (now - lastAstrosMs >= (unsigned long)ASTROS_REFRESH_SEC * 1000UL ||
        lastAstrosMs == 0) {
        // fetchAstros only updates peopleInSpace. Failures are tolerated
        // silently — Open Notify is known flaky and a -1 here shouldn't
        // disturb the rest of the ISS data.
        ISSData tmp = {};
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        memcpy(&tmp, &iss, sizeof(tmp));
        xSemaphoreGive(dataMutex);
        int prevCount = tmp.peopleInSpace;
        fetchAstros(tmp);
        if (tmp.peopleInSpace != prevCount && tmp.peopleInSpace >= 0) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            iss.peopleInSpace = tmp.peopleInSpace;
            xSemaphoreGive(dataMutex);
            lastAstrosMs = now;
        } else {
            scheduleRetry(lastAstrosMs, ASTROS_REFRESH_SEC);
        }
    }
    if (now - lastSpaceWxMs >= (unsigned long)SPACEWX_REFRESH_SEC * 1000UL ||
        lastSpaceWxMs == 0) {
        SpaceWxData tmp = {};
        fetchSpaceWx(tmp);
        if (tmp.ok) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            memcpy(&spwx, &tmp, sizeof(spwx));
            xSemaphoreGive(dataMutex);
            lastSpaceWxMs = now;
        } else {
            Serial.printf("[spwx] fail (%s) — retry in %ds\n",
                          tmp.error, FETCH_RETRY_AFTER_FAIL_SEC);
            scheduleRetry(lastSpaceWxMs, SPACEWX_REFRESH_SEC);
        }
    }
    if (now - lastLaunchMs >= (unsigned long)LAUNCH_REFRESH_SEC * 1000UL ||
        lastLaunchMs == 0) {
        LaunchData tmp = {};
        fetchLaunch(tmp);
        if (tmp.ok) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            memcpy(&launch, &tmp, sizeof(launch));
            xSemaphoreGive(dataMutex);
            lastLaunchMs = now;
        } else {
            Serial.printf("[launch] fail (%s) — retry in %ds\n",
                          tmp.error, FETCH_RETRY_AFTER_FAIL_SEC);
            scheduleRetry(lastLaunchMs, LAUNCH_REFRESH_SEC);
        }
    }
}

// ── Fetcher task — runs on core 0 ────────────────────
// Loops forever: maintain WiFi connection, refresh any data whose interval
// has elapsed, then sleep 500 ms. All HTTP blocking happens here, off the
// main loop, so touch and display stay responsive on core 1.
static void fetcherTask(void* arg) {
    Serial.printf("[fetcher] task started on core %d\n", xPortGetCoreID());
    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[wifi] dropped — silent reconnect");
            connectWiFiSilent();
        }
        maybeRefreshAll();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ── Touch handling (XPT2046 on VSPI) ─────────────────
// Press/release edge detection: page only flips once per physical tap,
// no matter how long the finger stays down. Touch zones are the entire
// left half (prev) and right half (next) — much bigger than the bottom
// nav strip, more forgiving for an under-glass case.
static void handleTouch() {
    static bool pressed = false;

    if (!ts.touched()) {
        pressed = false;
        return;
    }
    TS_Point p = ts.getPoint();

#if CALIBRATE_TOUCH
    if (!pressed) {
        Serial.printf("[touch raw] x=%d y=%d z=%d\n", p.x, p.y, p.z);
        pressed = true;
    }
    return;
#endif

    if (pressed) return;         // already handled this press
    if (millis() - lastTouchMs < 250) return;
    pressed = true;
    lastTouchMs = millis();

    int sx = map(p.x, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, 0, SCREEN_W);
    sx = constrain(sx, 0, SCREEN_W - 1);

    if (sx < SCREEN_W / 2) {
        currentPage = (currentPage - 1 + NUM_PAGES) % NUM_PAGES;
    } else {
        currentPage = (currentPage + 1) % NUM_PAGES;
    }
    forceRedraw = true;
    Serial.printf("[touch] x=%d  page -> %d\n", sx, currentPage);
}

// ── Setup ────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[BOOT] Clock + Weather + Space");

    halInit();
    uiInit();

    // Start XPT2046 on its own VSPI bus
    touchSPI.begin(XPT_CLK, XPT_MISO, XPT_MOSI, XPT_CS);
    ts.begin(touchSPI);
    ts.setRotation(SCREEN_ROT);
    Serial.println("[touch] XPT2046 started on VSPI");

#if CALIBRATE_TOUCH
    lcd.fillScreen(TFT_BLACK);
    lcd.setTextColor(TFT_WHITE);
    lcd.setTextSize(2);
    lcd.setCursor(10, 20);
    lcd.println("TOUCH CALIBRATE");
    lcd.setTextSize(1);
    lcd.setCursor(10, 60);
    lcd.println("Open Serial Monitor at 115200.");
    lcd.setCursor(10, 76);
    lcd.println("Tap each corner of the screen.");
    lcd.setCursor(10, 92);
    lcd.println("Record min/max X and min/max Y");
    lcd.setCursor(10, 108);
    lcd.println("from the logged raw values.");
    lcd.setCursor(10, 132);
    lcd.println("Paste into TOUCH_RAW_*_MIN/MAX");
    lcd.setCursor(10, 148);
    lcd.println("defines and set the flag to 0.");
    Serial.println("\n=== TOUCH CALIBRATION MODE ===");
    Serial.println("Tap each corner of the screen and watch the raw values.");
    Serial.println("Use the smallest and largest x,y seen.");
    while (1) {
        handleTouch();
        delay(20);
    }
#endif

    uiBootProgress(20, "Connecting WiFi...");
    if (!connectWiFi()) {
        uiError("WIFI FAILED", "Will keep retrying");
    }

    uiBootProgress(50, "Syncing time...");
    syncTime();

    // Create the data mutex BEFORE the first fetch so maybeRefreshAll()
    // can take/give it safely.
    dataMutex = xSemaphoreCreateMutex();

    uiBootProgress(80, "Fetching data...");
    maybeRefreshAll();   // initial fetch on core 1 so first frame has data

    uiBootProgress(100, "Ready");
    delay(300);

    forceRedraw = true;

    // Launch the background fetcher on core 0. From here on the main loop
    // (core 1) never touches the network — touch and display stay smooth
    // even during HTTPS round-trips.
    xTaskCreatePinnedToCore(
        fetcherTask,    // task function
        "fetcher",      // name
        8192,           // stack size (HTTPClient + ArduinoJson need a few KB)
        NULL,           // params
        1,              // priority (1 = above idle, below most system tasks)
        NULL,           // handle (not needed)
        0               // core 0 (Arduino's loop runs on core 1 by default)
    );
}

// ── Loop ─────────────────────────────────────────────
// Runs on core 1. No network I/O happens here — the fetcher task on core 0
// keeps `weather`, `iss`, `spwx`, `launch` fresh in the background.
void loop() {
    handleTouch();

    // calcSky is read+written only on this core, so no mutex needed.
    calcSky(sky);

    // Render once per second.
    static unsigned long lastTick = 0;
    if (forceRedraw || (millis() - lastTick >= 1000)) {
        lastTick = millis();
        struct tm t;
        getLocalTime(&t, 100);

        // Snapshot under mutex — held for microseconds, not the duration of
        // the render. Renderer works on local copies and never blocks the
        // fetcher (and vice versa).
        WeatherData  wSnap;
        ISSData      issSnap;
        SpaceWxData  spSnap;
        LaunchData   lnSnap;
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        memcpy(&wSnap,   &weather, sizeof(weather));
        memcpy(&issSnap, &iss,     sizeof(iss));
        memcpy(&spSnap,  &spwx,    sizeof(spwx));
        memcpy(&lnSnap,  &launch,  sizeof(launch));
        xSemaphoreGive(dataMutex);

        uiRender(currentPage, t, wSnap, issSnap, spSnap, sky, lnSnap,
                 WiFi.RSSI(), forceRedraw);
        forceRedraw = false;
    }

    delay(20);
}
