// ═══════════════════════════════════════════════════════
//  sky.ino — Moon phase + sky calculations (no network)
// ═══════════════════════════════════════════════════════

// Synodic month length in days
#define LUNATION_DAYS 29.530588853

// Reference new moon: 2000-01-06 18:14 UTC = JD 2451550.259722
#define NEW_MOON_REF_JD 2451550.259722

static const char* moonPhaseName(float age) {
    if (age < 1.84)  return "New";
    if (age < 5.53)  return "Waxing Crescent";
    if (age < 9.22)  return "First Quarter";
    if (age < 12.91) return "Waxing Gibbous";
    if (age < 16.61) return "Full";
    if (age < 20.30) return "Waning Gibbous";
    if (age < 23.99) return "Last Quarter";
    if (age < 27.68) return "Waning Crescent";
    return "New";
}

// Compute Julian Day from current UNIX time
static double currentJulianDay() {
    time_t now; time(&now);
    return (double)now / 86400.0 + 2440587.5;
}

void calcSky(SkyData& out) {
    double jd  = currentJulianDay();
    double age = fmod(jd - NEW_MOON_REF_JD, LUNATION_DAYS);
    if (age < 0) age += LUNATION_DAYS;
    out.moonAge   = (float)age;
    out.moonIllum = 50.0f * (1.0f - cosf(2.0f * (float)PI * out.moonAge / (float)LUNATION_DAYS));
    strlcpy(out.moonPhase, moonPhaseName(out.moonAge), sizeof(out.moonPhase));
}
