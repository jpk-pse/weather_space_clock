// ═══════════════════════════════════════════════════════
//  space.ino — Network fetchers for ISS, space weather, and launches
// ═══════════════════════════════════════════════════════

// Earth radius for haversine distance
#define EARTH_R_KM 6371.0

static double toRad(double deg) { return deg * (double)PI / 180.0; }

static float haversineKm(float lat1, float lon1, float lat2, float lon2) {
    double dLat = toRad(lat2 - lat1);
    double dLon = toRad(lon2 - lon1);
    double a = sin(dLat/2) * sin(dLat/2) +
               cos(toRad(lat1)) * cos(toRad(lat2)) *
               sin(dLon/2) * sin(dLon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return (float)(EARTH_R_KM * c);
}

// ── ISS position: wheretheiss.at (HTTPS, no key) ─────
void fetchISS(ISSData& out) {
    if (WiFi.status() != WL_CONNECTED) {
        strlcpy(out.error, "wifi_down", sizeof(out.error));
        out.ok = false;
        return;
    }
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;

    const char* url = "https://api.wheretheiss.at/v1/satellites/25544";
    if (!https.begin(client, url)) {
        strlcpy(out.error, "https_init", sizeof(out.error));
        out.ok = false; return;
    }
    https.setTimeout(API_TIMEOUT_MS);
    int code = https.GET();
    if (code != 200) {
        snprintf(out.error, sizeof(out.error), "http_%d", code);
        https.end(); out.ok = false; return;
    }
    String body = https.getString();
    https.end();

    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        strlcpy(out.error, "json", sizeof(out.error));
        out.ok = false; return;
    }
    out.lat   = doc["latitude"]  | 0.0f;
    out.lon   = doc["longitude"] | 0.0f;
    out.altKm = doc["altitude"]  | 0.0f;
    out.velKmH = doc["velocity"] | 0.0f;
    strlcpy(out.visibility, doc["visibility"] | "?", sizeof(out.visibility));
    out.distFromUserKm = haversineKm(out.lat, out.lon, LATITUDE, LONGITUDE);
    out.ok = true;

    Serial.printf("[iss] lat=%.2f lon=%.2f alt=%.1fkm vel=%.0fkm/h dist=%.0fkm\n",
                  out.lat, out.lon, out.altKm, out.velKmH, out.distFromUserKm);
}

// ── People in space: Open Notify (HTTP) ───────────────
// Open Notify is flaky; we only update peopleInSpace and never touch ok.
void fetchAstros(ISSData& out) {
    if (WiFi.status() != WL_CONNECTED) return;
    WiFiClient client;
    HTTPClient http;
    if (!http.begin(client, "http://api.open-notify.org/astros.json")) return;
    http.setTimeout(API_TIMEOUT_MS);
    int code = http.GET();
    if (code == 200) {
        String body = http.getString();
        JsonDocument doc;
        if (!deserializeJson(doc, body)) {
            out.peopleInSpace = doc["number"] | -1;
            Serial.printf("[astros] %d in space\n", out.peopleInSpace);
        }
    } else {
        Serial.printf("[astros] HTTP %d\n", code);
    }
    http.end();
}

// ── Space weather: NOAA SWPC ─────────────────────────
void fetchSpaceWx(SpaceWxData& out) {
    if (WiFi.status() != WL_CONNECTED) {
        strlcpy(out.error, "wifi_down", sizeof(out.error)); out.ok = false; return;
    }
    WiFiClientSecure client;
    client.setInsecure();

    // 1) Kp index — latest entry of NOAA planetary-k-index
    HTTPClient https;
    if (!https.begin(client, "https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json")) {
        strlcpy(out.error, "kp_init", sizeof(out.error)); out.ok = false; return;
    }
    https.setTimeout(API_TIMEOUT_MS);
    int code = https.GET();
    if (code != 200) {
        snprintf(out.error, sizeof(out.error), "kp_%d", code);
        https.end(); out.ok = false; return;
    }
    String body = https.getString();
    https.end();

    // Format: array of arrays [["time_tag","Kp","a_running","station_count"], ...]
    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, body);
    if (e) { strlcpy(out.error, "kp_json", sizeof(out.error)); out.ok = false; return; }

    JsonArray arr = doc.as<JsonArray>();
    int n = arr.size();
    if (n < 2) { strlcpy(out.error, "kp_empty", sizeof(out.error)); out.ok = false; return; }
    // Last row is the most recent data (row 0 is the header)
    JsonArray last = arr[n - 1];
    out.kp = atof(last[1] | "0");

    // 2) Solar wind plasma — most recent speed
    // Endpoint returns [["time","density","speed","temperature"], ...]
    HTTPClient https2;
    if (https2.begin(client, "https://services.swpc.noaa.gov/products/solar-wind/plasma-1-day.json")) {
        https2.setTimeout(API_TIMEOUT_MS);
        if (https2.GET() == 200) {
            JsonDocument d2;
            if (!deserializeJson(d2, https2.getString())) {
                JsonArray a2 = d2.as<JsonArray>();
                if (a2.size() > 1) {
                    JsonArray r = a2[a2.size() - 1];
                    out.windSpeed = atoi(r[2] | "0");
                }
            }
        }
        https2.end();
    }

    // 3) X-ray flare class — primary GOES, latest sample
    HTTPClient https3;
    if (https3.begin(client, "https://services.swpc.noaa.gov/json/goes/primary/xrays-1-day.json")) {
        https3.setTimeout(API_TIMEOUT_MS);
        if (https3.GET() == 200) {
            JsonDocument d3;
            if (!deserializeJson(d3, https3.getString())) {
                JsonArray a3 = d3.as<JsonArray>();
                // Find latest "0.1-0.8nm" energy entry — that's the flare-class band
                float peakFlux = 0;
                for (int i = a3.size() - 1; i >= 0 && i >= (int)a3.size() - 24; i--) {
                    JsonObject o = a3[i];
                    const char* energy = o["energy"] | "";
                    if (strstr(energy, "0.1-0.8")) {
                        float flux = o["flux"] | 0.0f;
                        if (flux > peakFlux) peakFlux = flux;
                    }
                }
                // Convert flux to class letter + magnitude
                // A: <1e-7, B: 1e-7..1e-6, C: 1e-6..1e-5, M: 1e-5..1e-4, X: >=1e-4
                char cls = 'A';
                float mant = 0;
                if      (peakFlux >= 1e-4f) { cls = 'X'; mant = peakFlux / 1e-4f; }
                else if (peakFlux >= 1e-5f) { cls = 'M'; mant = peakFlux / 1e-5f; }
                else if (peakFlux >= 1e-6f) { cls = 'C'; mant = peakFlux / 1e-6f; }
                else if (peakFlux >= 1e-7f) { cls = 'B'; mant = peakFlux / 1e-7f; }
                else                        { cls = 'A'; mant = peakFlux * 1e7f;  }
                snprintf(out.flareClass, sizeof(out.flareClass), "%c%.1f", cls, mant);
            }
        }
        https3.end();
    }

    out.ok = true;
    Serial.printf("[spwx] Kp=%.1f wind=%dkm/s flare=%s\n",
                  out.kp, out.windSpeed, out.flareClass);
}

// ── Next launch: Launch Library 2 (TheSpaceDevs) ─────
// Parse ISO 8601 "YYYY-MM-DDTHH:MM:SSZ" → epoch seconds (UTC)
//
// Compute UTC epoch directly from Y/M/D/h/m/s without touching TZ.
// (Earlier version swapped TZ to "UTC0" and tried to restore via the
//  getenv() pointer — but setenv() invalidates that buffer, so the
//  restore wrote garbage and the global TZ stayed UTC, breaking the
//  clock display. This version is pure arithmetic, side-effect free.)
static time_t parseIso8601(const char* s) {
    if (!s) return 0;
    int y, mo, d, h, m, sec;
    if (sscanf(s, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &m, &sec) != 6) return 0;
    if (mo < 1 || mo > 12) return 0;

    // Days from 1970-01-01 to start of year y (UTC)
    long days = 0;
    for (int yr = 1970; yr < y; yr++) {
        bool leap = (yr % 4 == 0 && yr % 100 != 0) || (yr % 400 == 0);
        days += leap ? 366 : 365;
    }
    static const int daysBeforeMonth[] = {0,31,59,90,120,151,181,212,243,273,304,334};
    days += daysBeforeMonth[mo - 1];
    bool curLeap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
    if (mo > 2 && curLeap) days += 1;
    days += (d - 1);

    return (time_t)days * 86400L + (time_t)h * 3600L + (time_t)m * 60L + sec;
}

void fetchLaunch(LaunchData& out) {
    if (WiFi.status() != WL_CONNECTED) {
        strlcpy(out.error, "wifi_down", sizeof(out.error)); out.ok = false; return;
    }
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;

    // Default mode includes rocket/pad detail we need.
    const char* url = "https://ll.thespacedevs.com/2.2.0/launch/upcoming/?limit=1";
    if (!https.begin(client, url)) {
        strlcpy(out.error, "init", sizeof(out.error)); out.ok = false; return;
    }
    https.setTimeout(API_TIMEOUT_MS);
    https.addHeader("User-Agent", "CYD-clock/1.0");
    int code = https.GET();
    if (code != 200) {
        snprintf(out.error, sizeof(out.error), "http_%d", code);
        https.end(); out.ok = false; return;
    }
    String body = https.getString();
    https.end();

    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, body);
    if (e) { strlcpy(out.error, "json", sizeof(out.error)); out.ok = false; return; }

    JsonArray results = doc["results"].as<JsonArray>();
    if (results.size() == 0) {
        strlcpy(out.error, "no_launches", sizeof(out.error));
        out.ok = false; return;
    }
    JsonObject l = results[0];
    strlcpy(out.name,     l["name"]                 | "?",   sizeof(out.name));
    strlcpy(out.provider, l["launch_service_provider"]["name"] | "?", sizeof(out.provider));
    strlcpy(out.vehicle,  l["rocket"]["configuration"]["name"] | l["rocket"]["full_name"] | "?", sizeof(out.vehicle));
    strlcpy(out.pad,      l["pad"]["name"]          | l["pad"]["location"]["name"] | "?", sizeof(out.pad));
    out.netEpoch = parseIso8601(l["net"] | "");

    out.ok = true;
    Serial.printf("[launch] %s @ %ld\n", out.name, (long)out.netEpoch);
}
