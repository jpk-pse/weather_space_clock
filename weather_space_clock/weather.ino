// ═══════════════════════════════════════════════════════
//  weather.ino — Open-Meteo current + daily forecast
//  No API key required.
// ═══════════════════════════════════════════════════════

// WMO weather code → short label.
// Reference: https://open-meteo.com/en/docs (WMO Weather interpretation codes)
const char* weatherCodeShort(int code) {
    if (code == 0)                    return "Clear";
    if (code == 1)                    return "Mostly Clear";
    if (code == 2)                    return "Partly Cloudy";
    if (code == 3)                    return "Overcast";
    if (code == 45 || code == 48)     return "Fog";
    if (code >= 51 && code <= 55)     return "Drizzle";
    if (code == 56 || code == 57)     return "Frz Drizzle";
    if (code >= 61 && code <= 65)     return "Rain";
    if (code == 66 || code == 67)     return "Frz Rain";
    if (code >= 71 && code <= 75)     return "Snow";
    if (code == 77)                   return "Snow Grains";
    if (code >= 80 && code <= 82)     return "Showers";
    if (code == 85 || code == 86)     return "Snow Showers";
    if (code == 95)                   return "Thunderstorm";
    if (code == 96 || code == 99)     return "T-storm/Hail";
    return "Unknown";
}

// Parse Open-Meteo ISO time string "YYYY-MM-DDTHH:MM" → hour, minute
static bool parseIsoHHMM(const char* s, int& hOut, int& mOut) {
    if (!s) return false;
    // Find 'T'
    const char* t = strchr(s, 'T');
    if (!t || strlen(t) < 6) return false;
    hOut = (t[1] - '0') * 10 + (t[2] - '0');
    mOut = (t[4] - '0') * 10 + (t[5] - '0');
    return true;
}

static void fmt12h(int h24, int m, char* out, size_t len) {
    const char* ampm = (h24 >= 12) ? "PM" : "AM";
    int h12 = h24 % 12;
    if (h12 == 0) h12 = 12;
    snprintf(out, len, "%d:%02d %s", h12, m, ampm);
}

void fetchWeather(WeatherData& out) {
    out.ok = false;

    if (WiFi.status() != WL_CONNECTED) {
        strlcpy(out.error, "wifi_down", sizeof(out.error));
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();  // Weather data isn't sensitive — skip cert pinning

    HTTPClient https;

    char url[512];
    snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast"
        "?latitude=%.4f&longitude=%.4f"
        "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m,is_day,apparent_temperature"
        "&daily=temperature_2m_max,temperature_2m_min,weather_code,sunrise,sunset"
        "&timezone=auto&temperature_unit=fahrenheit&wind_speed_unit=mph&forecast_days=2",
        LATITUDE, LONGITUDE);

    if (!https.begin(client, url)) {
        strlcpy(out.error, "https_init", sizeof(out.error));
        return;
    }
    https.setTimeout(API_TIMEOUT_MS);

    int code = https.GET();
    Serial.printf("[weather] HTTP %d\n", code);

    if (code != 200) {
        snprintf(out.error, sizeof(out.error), "http_%d", code);
        https.end();
        return;
    }

    String payload = https.getString();
    https.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        snprintf(out.error, sizeof(out.error), "json:%s", err.c_str());
        return;
    }

    JsonObject cur = doc["current"];
    JsonObject daily = doc["daily"];
    if (cur.isNull() || daily.isNull()) {
        strlcpy(out.error, "no_data", sizeof(out.error));
        return;
    }

    out.tempF       = cur["temperature_2m"]      | NAN;
    out.feelsF      = cur["apparent_temperature"] | NAN;
    out.humidity    = cur["relative_humidity_2m"] | 0;
    out.windMph     = cur["wind_speed_10m"]      | 0.0f;
    out.weatherCode = cur["weather_code"]        | -1;
    out.isDay       = (cur["is_day"]             | 1) != 0;

    out.todayHi     = daily["temperature_2m_max"][0] | NAN;
    out.todayLo     = daily["temperature_2m_min"][0] | NAN;
    out.tmrwHi      = daily["temperature_2m_max"][1] | NAN;
    out.tmrwLo      = daily["temperature_2m_min"][1] | NAN;
    out.tmrwCode    = daily["weather_code"][1]       | -1;

    const char* sr = daily["sunrise"][0] | "";
    const char* ss = daily["sunset"][0]  | "";
    int h, m;
    if (parseIsoHHMM(sr, h, m)) fmt12h(h, m, out.sunrise, sizeof(out.sunrise));
    else strlcpy(out.sunrise, "--", sizeof(out.sunrise));
    if (parseIsoHHMM(ss, h, m)) fmt12h(h, m, out.sunset,  sizeof(out.sunset));
    else strlcpy(out.sunset,  "--", sizeof(out.sunset));

    out.ok = true;
    Serial.printf("[weather] %.1fF code=%d hi=%.0f lo=%.0f\n",
                  out.tempF, out.weatherCode, out.todayHi, out.todayLo);
}
