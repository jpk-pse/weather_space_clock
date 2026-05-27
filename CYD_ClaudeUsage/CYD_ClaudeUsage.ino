/*
 * Claude Usage Monitor — CYD (ESP32-2432S028R)
 * =============================================
 * Displays your Claude Code 5-hour and 7-day usage on the
 * Cheap Yellow Display. No laptop server needed — polls
 * api.anthropic.com directly using your OAuth token.
 *
 * LIBRARIES NEEDED (Sketch > Include Library > Manage Libraries):
 *   - TFT_eSPI  by Bodmer
 *   - ArduinoJson  by Benoit Blanchon
 *
 * Also copy User_Setup.h into your TFT_eSPI library folder before compiling.
 *
 * BUTTONS:
 *   BOOT (GPIO 0)  — cycle digit during PIN / cycle brightness on dashboard
 *   GPIO35 button  — confirm digit during PIN / force refresh on dashboard
 *   Hold both on boot — factory reset
 */

// ── Includes (FS must come before WebServer) ──────────
#include <FS.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>
#include "esp_system.h"
#include "esp_mac.h"

// ── Structs (defined here so all tabs can use them) ───
struct UsageData {
    float    h5;
    float    d7;
    uint32_t h5ResetEpoch;
    uint32_t d7ResetEpoch;
    bool     ok;
    char     error[64];
};

struct EncryptedBlob {
    uint8_t  salt[6];
    uint16_t len;
    uint8_t  ciphertext[512];
};

// ── Color shortcuts used in main loop ─────────────────
#define C_DIM   0x7BEF
#define C_CRIT  0xF800

// ── Global LCD instance (used by all tabs) ────────────
TFT_eSPI lcd;

// ── Config ────────────────────────────────────────────
#define DEFAULT_POLL_SEC       120
#define MIN_POLL_SEC            30
#define MAX_POLL_SEC           300
#define MAX_PIN_ATTEMPTS        10   // unused, kept for future use
#define LOCKOUT_BASE_SEC        60   // unused, kept for future use
#define SCREEN_W               320
#define SCREEN_H               240
#define SCREEN_ROT               1
#define DEFAULT_BRIGHTNESS       2
#define WIFI_CONNECT_TIMEOUT_S  20
#define API_TIMEOUT_MS       15000
#define MESSAGES_ENDPOINT    "https://api.anthropic.com/v1/messages"
#define ANTHROPIC_VERSION    "2023-06-01"
#define PROBE_MODEL          "claude-haiku-4-5-20251001"
#define NVS_NAMESPACE        "claude"

// ── HAL pin definitions (CYD) ─────────────────────────
#define BTN_A_PIN   0    // BOOT button (active LOW)
#define BTN_B_PIN  35    // Side button (active LOW)
#define BL_PIN     21    // Backlight PWM

// ── Shared state ──────────────────────────────────────
static Preferences   prefs;
static char          token[256];
static unsigned long lastFetch  = 0;
static int           pollMs     = DEFAULT_POLL_SEC * 1000;
static uint8_t       brightness = DEFAULT_BRIGHTNESS;
static UsageData     lastUsage;

static bool prevA = false, prevB = false;
static bool tapA  = false, tapB  = false;

// ── HAL ───────────────────────────────────────────────
void halInit() {
    lcd.init();
    lcd.setRotation(SCREEN_ROT);
    pinMode(BTN_A_PIN, INPUT_PULLUP);
    pinMode(BTN_B_PIN, INPUT_PULLUP);
    // ESP32 Arduino 3.x: ledcAttach replaces ledcSetup+ledcAttachPin
    ledcAttach(BL_PIN, 5000, 8);
    ledcWrite(BL_PIN, 160);
}

void halUpdate() {
    bool a = !digitalRead(BTN_A_PIN);
    bool b = !digitalRead(BTN_B_PIN);
    tapA = (a && !prevA);
    tapB = (b && !prevB);
    prevA = a; prevB = b;
}

bool halBtnAWasPressed() { bool r = tapA; tapA = false; return r; }
bool halBtnBWasPressed() { bool r = tapB; tapB = false; return r; }
bool halBtnAIsPressed()  { return !digitalRead(BTN_A_PIN); }
bool halBtnBIsPressed()  { return !digitalRead(BTN_B_PIN); }

void halSetBrightness(uint8_t level) {
    static const uint8_t vals[] = {0, 60, 160, 255};
    ledcWrite(BL_PIN, vals[level < 4 ? level : 2]);
}

// ── Helpers ───────────────────────────────────────────
static bool connectWiFi(const char* ssid, const char* pass) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    int ticks = 0;
    while (WiFi.status() != WL_CONNECTED) {
        ticks++;
        uiConnecting(ssid, ticks / 2);
        delay(500);
        if (ticks > WIFI_CONNECT_TIMEOUT_S * 2) return false;
    }
    return true;
}

static void syncTime() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    struct tm t;
    getLocalTime(&t, 5000);
}

static void refresh() {
    if (WiFi.status() != WL_CONNECTED) {
        prefs.begin(NVS_NAMESPACE, true);
        String s = prefs.getString("ssid", "");
        String p = prefs.getString("wifipass", "");
        prefs.end();
        connectWiFi(s.c_str(), p.c_str());
    }
    fetchUsage(token, lastUsage);
    lastFetch = millis();
    uiDashboard(lastUsage, lastFetch, WiFi.RSSI());
}

static void enterPin(char* pinOut, int maxLen) {} // unused, kept for compile compat

// ── Setup ─────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    halInit();
    uiInit();

    uiBootProgress(10, "Initializing...");
    delay(300);
    uiBootProgress(30, "Checking config...");
    delay(200);

    halUpdate();
    if (halBtnAIsPressed() && halBtnBIsPressed()) {
        uiBootProgress(50, "Factory reset...");
        prefs.begin(NVS_NAMESPACE, false);
        prefs.clear();
        prefs.end();
        uiError("NVS WIPED", "Rebooting in 2s...");
        delay(2000);
        ESP.restart();
    }

    prefs.begin(NVS_NAMESPACE, true);
    bool provisioned = prefs.getBool("provisioned", false);
    prefs.end();

    if (!provisioned) {
        uiBootProgress(50, "No config found");
        delay(400);

        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        char apName[24];
        snprintf(apName, sizeof(apName), "ClaudeMonitor-%02X%02X", mac[4], mac[5]);

        static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
        uint8_t rnd[8];
        esp_fill_random(rnd, sizeof(rnd));
        char apPass[9];
        for (int i = 0; i < 8; i++) apPass[i] = alphabet[rnd[i] % (sizeof(alphabet) - 1)];
        apPass[8] = '\0';

        runProvisioningPortal(apName, apPass);
        return;
    }

    uiBootProgress(50, "Config loaded");
    delay(200);

    prefs.begin(NVS_NAMESPACE, true);
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("wifipass", "");
    EncryptedBlob blob;
    prefs.getBytes("blob", &blob, sizeof(blob));
    pollMs     = prefs.getInt("poll_sec", DEFAULT_POLL_SEC) * 1000;
    brightness = prefs.getInt("brightness", DEFAULT_BRIGHTNESS);
    prefs.end();

    halSetBrightness(brightness);

    if (!decryptToken(blob, token, sizeof(token))) {
        uiError("TOKEN ERROR", "Re-provision the device");
        delay(5000);
        ESP.restart();
    }

    uiBootProgress(70, "Connecting WiFi...");
    if (!connectWiFi(ssid.c_str(), pass.c_str())) {
        uiError("WIFI FAILED", ssid.c_str());
        delay(5000);
        ESP.restart();
    }

    uiBootProgress(90, "Syncing time...");
    syncTime();

    uiBootProgress(95, "Fetching usage...");
    refresh();
}

// ── Loop ──────────────────────────────────────────────
void loop() {
    halUpdate();

    // ── BOOT button: tap = brightness, 5s hold = reset ─
    static unsigned long bootHeldSince = 0;
    static bool          inHold        = false;
    static bool          holdFired     = false;
    static int           lastCountdown = -1;

    if (halBtnAIsPressed()) {
        if (!inHold) {
            bootHeldSince = millis();
            inHold        = true;
            holdFired     = false;
            lastCountdown = -1;
        }

        unsigned long held      = millis() - bootHeldSince;
        int           countdown = 5 - (int)(held / 1000);

        if (!holdFired) {
            if (held >= 5000) {
                // 5 seconds reached — factory reset
                holdFired = true;
                lcd.fillScreen(TFT_BLACK);
                lcd.setTextColor(C_CRIT, TFT_BLACK);
                lcd.setTextSize(2);
                lcd.setCursor(10, 80);
                lcd.print("FACTORY RESET");
                lcd.setTextColor(C_DIM, TFT_BLACK);
                lcd.setTextSize(1);
                lcd.setCursor(10, 110);
                lcd.print("Wiping credentials...");
                prefs.begin(NVS_NAMESPACE, false);
                prefs.clear();
                prefs.end();
                delay(1500);
                ESP.restart();
            } else if (held >= 500 && countdown != lastCountdown) {
                // Update footer countdown only when the second changes
                lastCountdown = countdown;
                lcd.fillRect(0, SCREEN_H - 16, SCREEN_W, 16, C_CRIT);
                lcd.setTextColor(TFT_WHITE, C_CRIT);
                lcd.setTextSize(1);
                char msg[48];
                snprintf(msg, sizeof(msg), "  Keep holding %ds to factory reset...", countdown);
                lcd.setCursor(4, SCREEN_H - 11);
                lcd.print(msg);
            }
        }
    } else {
        if (inHold) {
            unsigned long held = millis() - bootHeldSince;
            if (!holdFired) {
                if (held < 500) {
                    // Clean tap — cycle brightness
                    brightness = (brightness + 1) % 4;
                    halSetBrightness(brightness);
                }
                // Released before 5s — brief pause then restore footer
                delay(200);
                lcd.fillRect(0, SCREEN_H - 16, SCREEN_W, 16, 0x1082);
                lcd.setTextColor(C_DIM, 0x1082);
                lcd.setTextSize(1);
                lcd.setCursor(6, SCREEN_H - 11);
                lcd.print("BOOT=brightness  BTN=refresh now");
            }
            inHold    = false;
            holdFired = false;
        }
    }

    if (halBtnBWasPressed()) {
        refresh();
    }

    if (millis() - lastFetch >= (unsigned long)pollMs) {
        refresh();
    }

    // Redraw every 10s to update the "Xs ago" header
    static unsigned long lastRedraw = 0;
    if (!inHold && millis() - lastRedraw > 10000) {
        uiDashboard(lastUsage, lastFetch, WiFi.RSSI());
        lastRedraw = millis();
    }

    delay(20);
}
