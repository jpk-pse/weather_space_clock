# Weather + Space Clock (WSC) — Software Requirements Specification

**Document ID:** WSC-SPEC-001
**Tool:** Weather + Space Clock (WSC)
**Target firmware version:** 1.1.0
**Status:** Rev 1-draft

## Revision History

| Revision | Date | Author | Summary |
|---|---|---|---|
| Rev 1-draft | 2026-06-12 | James Krueger | Descriptive specification of firmware v1.1.0 — as-built v1.0.0 plus the in-session resolutions of OI-004 (wind speed now displayed; unused `bz` field removed) and OI-003 (TLS-insecure accepted). |

---

## 1. Introduction

### 1.1 Purpose

This document specifies the functional and non-functional requirements of the **Weather + Space Clock (WSC)**, a touchscreen desk display built on the ESP32-2432S028R "Cheap Yellow Display" (CYD). The specification descriptively documents the behavior of firmware version 1.1.0 so that the device can be maintained, verified, and extended against a single authoritative reference.

### 1.2 Scope

WSC is a single-user, bare-metal embedded device that presents five touch-navigable information pages — Clock+Weather, ISS Live, Space Weather, Sky Tonight, and Next Launch — using free public APIs that require no API key, together with network time. The device emphasizes touch responsiveness through staggered non-blocking data refreshes and degrades softly on every failure without rebooting itself.

**In scope:**
- Five display pages and a shared touchable bottom navigation bar.
- Acquisition and display of weather, ISS position, space-weather, sky/moon, and launch data.
- Weather-code-driven color theming of the Clock+Weather page.
- Network time synchronization and local-time display.
- One-time touch calibration workflow.
- Soft-failure handling and WiFi reconnection.

**Out of scope:**
- Over-the-air (OTA) firmware updates.
- Any on-device or web-based configuration user interface.
- Multi-device synchronization.
- Persistence, logging, or historical data retention.
- User authentication or multi-user roles.

### 1.3 Document Identification

This specification is identified as **WSC-SPEC-001**. This document identifier is distinct from the requirement identifiers defined in §1.5; "SPEC" is a document-level designator and is not a requirement TYPE code. The canonical source is `Specs/WSC_Spec.md`; the released `.docx` is a generated artifact. Revisions are integers starting at Rev 1; working drafts are denoted `Rev N-draft` in the revision history above.

### 1.4 Intended Audience

The intended audience is the device owner-operator who builds, flashes, calibrates, and maintains the unit, and any developer extending the firmware. Readers are assumed to be familiar with the Arduino IDE, ESP32 hardware, and basic embedded C++; no avionics or enterprise-software background is assumed.

### 1.5 Document Conventions

Requirement keywords are used as follows: **shall** denotes a mandatory requirement; **should** denotes a recommendation; **will** denotes a statement of fact; **may** denotes a permitted option. Each requirement states one testable condition.

Requirement identifiers use the format **`WSC-[TYPE]-[NNNN]`**, where TYPE is exactly one of F (functional), NF (non-functional), UI (user interface), or IF (interface), and NNNN is a four-digit zero-padded sequence number (0001–9999). These four TYPE codes are the complete set; no other TYPE values are used for requirements. Identifiers are immutable and are not reused.

Verification methods are abbreviated **T** (Test), **A** (Analysis), **I** (Inspection), and **D** (Demonstration). Every functional requirement carries an implementation-status tag whose permitted values are **MVP** and **Post-MVP**; because firmware v1.1.0 ships all specified features, functional requirements are tagged **MVP** unless explicitly noted as **Post-MVP**.

---

## 2. Applicable Documents

### 2.1 Normative References

Normative references define interfaces, hardware, or toolchain components on which the firmware directly depends. The behavior specified in this document is valid only against the versions identified below.

| Ref ID | Resource | Version / Identifier | Locator | Used for |
|---|---|---|---|---|
| N-1 | Open-Meteo Forecast API | v1 (keyless) | `https://open-meteo.com/` | Current weather, daily forecast, sunrise/sunset, weather codes |
| N-2 | WhereTheISS.at API | v1 (keyless) | `https://wheretheiss.at/w/developer` | ISS latitude, longitude, altitude, velocity |
| N-3 | Open Notify — Astros API | (keyless, HTTP only) | `http://open-notify.org/Open-Notify-API/People-In-Space/` | Count of people currently in space |
| N-4 | NOAA SWPC products | (keyless JSON products) | `https://www.swpc.noaa.gov/` | Planetary Kp index, solar wind speed, X-ray flare class |
| N-5 | Launch Library 2 (The Space Devs) | v2.2.0 (keyless) | `https://thespacedevs.com/llapi` | Next orbital launch: provider, vehicle, mission, T-0, pad |
| N-6 | NTP time service | RFC 5905 | `pool.ntp.org`, `time.nist.gov`, `time.google.com` | Wall-clock time synchronization |
| N-7 | ESP32-2432S028R "Cheap Yellow Display" (CYD) | 2USB hardware revision | Vendor datasheet | Target hardware: MCU, 2.8" ILI9341 TFT, XPT2046 touch |
| N-8 | Arduino-ESP32 board support package | Espressif `esp32` core | `https://github.com/espressif/arduino-esp32` | Compilation target; partition scheme "Huge APP (3MB No OTA)" |
| N-9 | Arduino IDE | 2.x | `https://www.arduino.cc/en/software` | Build and flash toolchain |
| N-10 | TFT_eSPI library (Bodmer) | (per `User_Setup.h` in repo) | Arduino Library Manager | Display driver and rendering primitives |
| N-11 | ArduinoJson library (Benoit Blanchon) | v7.x | Arduino Library Manager | Parsing of all API JSON responses |
| N-12 | XPT2046_Touchscreen library (Paul Stoffregen) | (Library Manager release) | Arduino Library Manager | Resistive touch on the separate VSPI bus |

### 2.2 Informative References

Informative references provide background and are not binding on the implementation.

| Ref ID | Resource | Locator | Relevance |
|---|---|---|---|
| I-1 | Claude-Monitor (upstream fork origin) | `https://github.com/nimnim111/Claude-Monitor` | Original project from which WSC was forked and rewritten end-to-end |
| I-2 | WSC project README | `README.md` (repo root) | Build, library install, calibration, and board-setup procedure |
| I-3 | CYD 2.8" enclosure models | `https://www.printables.com/` | Optional 3D-printed case (search "CYD 2.8 case") |

Where a normative reference omits a precise version, the binding version is the one recorded in the repository (`User_Setup.h`, the sketch's library includes, and the documented board/partition settings) at firmware tag `v1.1.0`.

The Sky Tonight page is intentionally omitted from the normative references above: its moon-phase and sky data are computed entirely on-device from the synchronized clock (N-6) with no external resource, so no separate normative reference applies. All other display pages bind to the resource(s) listed above.

---

## 3. Definitions and Abbreviations

### 3.1 Definitions

| Term | Definition |
|---|---|
| Page | One of the five full-screen views the device cycles through; the unit of navigation. |
| Nav bar | The fixed bottom strip (height 22 px) showing the current page label and position dots. It is a position indicator, not the touch target: navigation taps are accepted across the entire left and right screen halves (see §4.2). |
| Theme | A named color palette (background, accent, secondary, header colors) applied to a page. The Clock+Weather page selects its theme from the current weather; space pages use a fixed or Kp-modulated palette. |
| Weather code | The World Meteorological Organization (WMO) numeric weather code returned by Open-Meteo, mapped on-device to a short condition label, an animated icon, and a theme. |
| Soft failure | A data-acquisition or connectivity failure that the firmware absorbs without rebooting, displaying "(loading…)" or the last good value instead. |
| ISS visibility state | The visibility field returned verbatim by the WhereTheISS.at API (N-2), with values `daylight` or `eclipsed`. When the API omits the field, the firmware substitutes `?`. The firmware does not transform this value. |
| Last good value | The most recently successful reading for a data field, retained and displayed when a subsequent fetch fails. |
| Refresh cadence | The fixed per-data-source interval at which the firmware attempts a new fetch (see §3.3.7). |
| Calibration mode | A build-time mode (`CALIBRATE_TOUCH = 1`) that prints raw touch coordinates to serial so the operator can record touch extents. |
| Moon age | Days elapsed since the most recent new moon, 0–29.53, computed on-device from Julian Day. |
| Synodic month (lunation) | The new-moon-to-new-moon lunar cycle, 29.530588853 days, used as the moon-age basis. |

### 3.2 Abbreviations and Acronyms

| Abbreviation | Expansion |
|---|---|
| CYD | Cheap Yellow Display — the ESP32-2432S028R board |
| ESP32 | Espressif dual-core Xtensa LX6 microcontroller with integrated WiFi |
| ILI9341 | TFT LCD display controller IC used on the CYD |
| ISS | International Space Station (NORAD catalog no. 25544) |
| JD | Julian Day — astronomical continuous day count |
| Kp | Planetary K-index — geomagnetic activity, scale 0–9 |
| NTP | Network Time Protocol |
| OTA | Over-the-Air (firmware update) — out of scope |
| RGB565 | 16-bit color encoding (5 red, 6 green, 5 blue) used by the display |
| SWPC | Space Weather Prediction Center (NOAA) |
| TFT | Thin-Film-Transistor LCD |
| TZ | Time zone (POSIX TZ string) |
| VSPI | The ESP32 hardware SPI bus instance dedicated to the touch controller |
| WMO | World Meteorological Organization (weather-code standard) |
| XPT2046 | Resistive touchscreen controller IC |

### 3.3 Enumerations

#### 3.3.1 Display Pages

Pages are indexed 0–4 in fixed cycle order. There are exactly five pages.

| Index | Identifier | Nav-bar label | Content |
|---|---|---|---|
| 0 | PAGE_CLOCK | `CLOCK` | Clock + Weather |
| 1 | PAGE_ISS | `ISS` | ISS Live |
| 2 | PAGE_SPACE_WX | `SPACE WX` | Space Weather |
| 3 | PAGE_SKY | `SKY` | Sky Tonight |
| 4 | PAGE_LAUNCH | `LAUNCH` | Next Launch |

#### 3.3.2 Weather Condition Labels

The WMO weather code maps to exactly one short label. Any code outside the listed sets yields `Unknown`.

| WMO code(s) | Label |
|---|---|
| 0 | `Clear` |
| 1 | `Mostly Clear` |
| 2 | `Partly Cloudy` |
| 3 | `Overcast` |
| 45, 48 | `Fog` |
| 51–55 | `Drizzle` |
| 56, 57 | `Frz Drizzle` |
| 61–65 | `Rain` |
| 66, 67 | `Frz Rain` |
| 71–75 | `Snow` |
| 77 | `Snow Grains` |
| 80–82 | `Showers` |
| 85, 86 | `Snow Showers` |
| 95 | `Thunderstorm` |
| 96, 99 | `T-storm/Hail` |
| any other | `Unknown` |

#### 3.3.3 Weather Theme Palettes (Clock+Weather page)

The Clock+Weather page selects a theme from the weather code; Clear and Cloudy groups have distinct day and night variants. The exact RGB565 values are specified in §8.8.

| WMO code(s) | Theme group | Day/Night variants |
|---|---|---|
| 0, 1 | Clear | Day, Night |
| 2, 3 | Cloudy | Day, Night |
| 45, 48 | Fog | single |
| 51–67, 80–82 | Rain | single |
| 71–77, 85, 86 | Snow | single |
| 95–99 | Thunderstorm | single |
| any other | Fog (default) | single |

Any weather code not matched by the rows above — including codes that resolve to `Unknown` in §3.3.2 — selects the Fog palette as its default theme.

#### 3.3.4 Kp Index Activity Levels

| Kp range | Level label |
|---|---|
| 0 ≤ Kp < 3 | `QUIET` |
| 3 ≤ Kp < 5 | `ACTIVE` |
| 5 ≤ Kp < 7 | `AURORA` |
| 7 ≤ Kp ≤ 9 | `STORM` |

#### 3.3.5 Solar Flare Classes

Flare class is derived from GOES X-ray flux and rendered as a letter plus mantissa (e.g., `M2.5`). The five letter classes are exhaustive over the flux domain.

| Class | X-ray flux (W·m⁻²) |
|---|---|
| `A` | flux < 1×10⁻⁷ |
| `B` | 1×10⁻⁷ ≤ flux < 1×10⁻⁶ |
| `C` | 1×10⁻⁶ ≤ flux < 1×10⁻⁵ |
| `M` | 1×10⁻⁵ ≤ flux < 1×10⁻⁴ |
| `X` | flux ≥ 1×10⁻⁴ |

#### 3.3.6 Moon Phase Names

Moon age (days) maps to exactly one of eight phase names; age wraps at the synodic month back to `New`.

| Moon age (days) | Phase name |
|---|---|
| 0 ≤ age < 1.84 | `New` |
| 1.84 ≤ age < 5.53 | `Waxing Crescent` |
| 5.53 ≤ age < 9.22 | `First Quarter` |
| 9.22 ≤ age < 12.91 | `Waxing Gibbous` |
| 12.91 ≤ age < 16.61 | `Full` |
| 16.61 ≤ age < 20.30 | `Waning Gibbous` |
| 20.30 ≤ age < 23.99 | `Last Quarter` |
| 23.99 ≤ age < 27.68 | `Waning Crescent` |
| 27.68 ≤ age < 29.53 | `New` (wraps) |

#### 3.3.7 Refresh Cadence Constants

| Constant | Value | Data source |
|---|---|---|
| `WEATHER_REFRESH_SEC` | 900 s (15 min) | Open-Meteo weather/forecast |
| `ISS_REFRESH_SEC` | 30 s | ISS position |
| `ASTROS_REFRESH_SEC` | 3600 s (1 h) | People in space |
| `SPACEWX_REFRESH_SEC` | 3600 s (1 h) | NOAA SWPC Kp / wind / flare |
| `LAUNCH_REFRESH_SEC` | 3600 s (1 h) | Next launch |
| `FETCH_RETRY_AFTER_FAIL_SEC` | 60 s | Retry delay after any failed fetch |

---

## 4. Functional Requirements

All requirements in §4 have status **Draft** and traceability **Derived** (descriptive of firmware v1.1.0) unless stated otherwise. Verification methods: T = Test, A = Analysis, I = Inspection, D = Demonstration. All §4 requirements are tagged **MVP** because v1.1.0 ships every feature.

### §4.1 System Startup and Connectivity — WSC-F-0001 through WSC-F-0013, 13 requirements

| ID | Requirement | Rationale | V | MVP |
|---|---|---|---|---|
| WSC-F-0001 | On power-on, the device shall initialize the display, backlight, and touch controller before attempting any network operation. | The UI must be able to render boot progress before connectivity. | D | MVP |
| WSC-F-0002 | The device shall set the display backlight to a fixed PWM duty of 200 of 255 at startup. | Matches `BRIGHTNESS_PWM`; fixed brightness, no runtime dimming. | I | MVP |
| WSC-F-0003 | The device shall initialize the XPT2046 touch controller on a dedicated VSPI bus separate from the display SPI bus. | The 2USB CYD wires touch to its own bus; the shared-bus path does not work. | T | MVP |
| WSC-F-0004 | During startup the device shall display boot-progress states at 20 %, 50 %, 80 %, and 100 % corresponding to WiFi connect, time sync, initial data fetch, and ready. | Gives the operator visible startup feedback. | D | MVP |
| WSC-F-0005 | The device shall connect to WiFi in station mode using the SSID and passphrase compiled from `secrets.h`. | Credentials are build-time, not user-entered on device. | T | MVP |
| WSC-F-0006 | The device shall abandon the initial WiFi connection attempt after 30 seconds without an association. | Matches `WIFI_CONNECT_TIMEOUT_S`; bounds boot time on a dead network. | T | MVP |
| WSC-F-0007 | If the initial WiFi connection fails, the device shall display a "WIFI FAILED" message and continue startup without rebooting. | Soft-failure model; the background task retries connection later. | D | MVP |
| WSC-F-0008 | After WiFi connects, the device shall synchronize time over NTP using the configured POSIX time-zone string and the three configured NTP servers. | Provides local wall-clock time; see N-6. | T | MVP |
| WSC-F-0009 | The device shall wait up to 8 seconds for the initial NTP time acquisition. | Matches the `getLocalTime` timeout; bounds boot delay if NTP is slow. | T | MVP |
| WSC-F-0010 | The device shall perform one synchronous fetch of all data sources before rendering the first page. | Ensures the first displayed frame contains real data, not placeholders. | D | MVP |
| WSC-F-0011 | After the initial fetch, the device shall launch a background data-fetcher task pinned to processor core 0. | Decouples all blocking network I/O from the display/touch loop on core 1. | T | MVP |
| WSC-F-0012 | The background fetcher task shall re-establish a dropped WiFi connection automatically, without rebooting the device. | Connectivity self-heals; no `ESP.restart()`. | D | MVP |
| WSC-F-0013 | WiFi reconnection performed by the background task shall not perform any display rendering. | TFT_eSPI is not multi-thread-safe; only core 1 may draw. | A | MVP |

### §4.2 Navigation and Touch Input — WSC-F-0014 through WSC-F-0024, 11 requirements

| ID | Requirement | Rationale | V | MVP |
|---|---|---|---|---|
| WSC-F-0014 | A tap anywhere on the left half of the screen shall navigate to the previous page. | Full-half zones are larger than the nav strip and forgiving under glass. | D | MVP |
| WSC-F-0015 | A tap anywhere on the right half of the screen shall navigate to the next page. | Symmetric with the previous-page zone. | D | MVP |
| WSC-F-0016 | Page navigation shall wrap cyclically in both directions. | Previous from the first page reaches the last; next from the last reaches the first. | D | MVP |
| WSC-F-0017 | The device shall register at most one page change per physical touch press. | Press/release edge detection; holding the finger down does not repeat. | T | MVP |
| WSC-F-0018 | The device shall ignore any tap occurring within 250 ms of the previously registered tap. | Debounce against contact bounce and double-triggers. | T | MVP |
| WSC-F-0019 | The device shall map the raw touch X coordinate to a screen X coordinate using the calibrated raw-X range before selecting the navigation half. | Calibration makes the half-boundary land at the physical screen center. | A | MVP |
| WSC-F-0020 | On any page change, the device shall perform a full redraw of the newly selected page. | The new page must replace the prior page's contents completely. | D | MVP |
| WSC-F-0021 | The device shall display a fixed bottom navigation bar showing the current page label and a page-position indicator. | Orients the user; visual detail in §8. | I | MVP |
| WSC-F-0022 | When compiled with touch calibration enabled, the device shall log raw touch coordinates to the serial port at 115200 baud. | One-time calibration workflow records the raw touch extents. | T | MVP |
| WSC-F-0023 | When compiled with touch calibration enabled, the device shall display on-screen instructions for recording the touch coordinate extents. | Guides the operator through calibration without external docs. | D | MVP |
| WSC-F-0024 | When compiled with touch calibration enabled, the device shall perform no page navigation. | Navigation is suppressed so taps serve only to report raw coordinates. | T | MVP |

The navigation half-boundary is the screen horizontal midpoint: a mapped X strictly less than half the screen width selects the previous page, and a mapped X at or beyond the midpoint selects the next page.

### §4.3 Clock + Weather Page — WSC-F-0025 through WSC-F-0042 and WSC-F-0100, 19 requirements

This page (PAGE_CLOCK, the default page shown first) always renders the clock, date, and WiFi signal; the weather block renders only when weather data is available.

| ID | Requirement | Rationale | V | MVP |
|---|---|---|---|---|
| WSC-F-0025 | The Clock+Weather page shall display the current local time in 12-hour format with an AM/PM indicator. | Primary clock readout. | D | MVP |
| WSC-F-0026 | The page shall toggle the time separator on and off once per second. | Blinking colon indicates the clock is live; visible on even seconds. | D | MVP |
| WSC-F-0027 | The page shall display the current date as weekday, abbreviated month, day, and four-digit year. | Date context for the clock. | D | MVP |
| WSC-F-0028 | The page shall display the current WiFi signal strength in dBm. | Connectivity feedback to the operator. | D | MVP |
| WSC-F-0029 | When weather data is available, the page shall display the current temperature in whole degrees Fahrenheit. | Primary weather readout. | D | MVP |
| WSC-F-0030 | When weather data is available, the page shall display the current weather condition label defined in §3.3.2. | Human-readable condition. | D | MVP |
| WSC-F-0031 | When weather data is available, the page shall display the feels-like temperature in whole degrees Fahrenheit. | Apparent temperature. | D | MVP |
| WSC-F-0032 | When weather data is available, the page shall display the relative humidity as a whole percentage. | Humidity readout. | D | MVP |
| WSC-F-0033 | When weather data is available, the page shall display today's high and low temperatures in whole degrees Fahrenheit. | Daily range; rendered as a single "H.. L.." line. | D | MVP |
| WSC-F-0034 | When weather data is available, the page shall display today's sunrise time. | Sun timing. | D | MVP |
| WSC-F-0035 | When weather data is available, the page shall display today's sunset time. | Sun timing. | D | MVP |
| WSC-F-0036 | When weather data is available, the page shall display an animated weather icon corresponding to the current weather code and day/night state, advancing one animation frame per second. | Visual condition cue selected by the weather code per §3.3.2 and day/night state. | D | MVP |
| WSC-F-0037 | When weather data is available, the page shall display tomorrow's forecast condition label defined in §3.3.2. | Next-day outlook. | D | MVP |
| WSC-F-0038 | When weather data is available, the page shall display tomorrow's high and low temperatures in whole degrees Fahrenheit. | Next-day range; rendered as a single "H.. L.." line. | D | MVP |
| WSC-F-0039 | The page shall apply the color theme selected from the current weather code and day/night state defined in §3.3.3. | Weather-driven re-tinting of the page. | D | MVP |
| WSC-F-0040 | When weather data is unavailable, the page shall display the text "(loading weather...)" in place of the weather block while continuing to display the clock and date. | Soft-failure placeholder; clock remains useful. | D | MVP |
| WSC-F-0041 | When weather data is unavailable, the page shall determine day/night state for theme selection from the local hour, treating 06:00 through 18:59 as day. | Provides a sensible theme before any weather fetch succeeds. | A | MVP |
| WSC-F-0042 | The page shall render any temperature value that is not a number as "--". | Avoids displaying garbage for missing readings. | T | MVP |
| WSC-F-0100 | When weather data is available, the page shall display the current wind speed in whole miles per hour, on the same line as the relative humidity. | Surfaces the wind reading the firmware already fetches (resolves OI-004). | D | MVP |

### §4.4 ISS Live Page — WSC-F-0043 through WSC-F-0050, 8 requirements

WSC-F-0043 specifies the header bar common to all four space-data pages (ISS Live, Space Weather, Sky Tonight, Next Launch) and is referenced by §4.5 through §4.7; the remaining requirements are specific to the ISS Live page. The "ISS visibility state" displayed by WSC-F-0049/0050 is defined in §3.1.

| ID | Requirement | Rationale | V | MVP |
|---|---|---|---|---|
| WSC-F-0043 | Each of the four space-data pages shall render a header bar showing the page title at the left and the current WiFi signal strength in dBm at the right. | Common chrome; orients the user and shows connectivity. | I | MVP |
| WSC-F-0044 | When ISS data is unavailable, the ISS Live page shall display the text "(no ISS data)". | Soft-failure placeholder for the page. | D | MVP |
| WSC-F-0045 | When ISS data is available, the page shall display the ISS latitude and longitude in degrees to two decimal places. | Primary position readout, labeled "latitude, longitude". | D | MVP |
| WSC-F-0046 | When ISS data is available, the page shall display the ISS altitude in whole kilometers. | Orbital altitude readout. | D | MVP |
| WSC-F-0047 | When ISS data is available, the page shall display the ISS orbital velocity in whole kilometers per hour. | Orbital speed readout. | D | MVP |
| WSC-F-0048 | When ISS data is available, the page shall display the great-circle distance from the configured device location to the ISS ground point in whole kilometers. | Localized "how far overhead" readout, computed on-device. | D | MVP |
| WSC-F-0049 | When the people-in-space count is available, the page shall display that count together with the ISS visibility state. | Combined crew-count and visibility line. | D | MVP |
| WSC-F-0050 | When the people-in-space count is unavailable, the page shall display the ISS visibility state without a count. | Open Notify is flaky; a missing count must not blank the line. | D | MVP |

### §4.5 Space Weather Page — WSC-F-0051 through WSC-F-0059, 9 requirements

The Space Weather page renders the common header bar (WSC-F-0043). Its data fields follow.

| ID | Requirement | Rationale | V | MVP |
|---|---|---|---|---|
| WSC-F-0051 | When space-weather data is unavailable, the Space Weather page shall display the text "(no data)". | Soft-failure placeholder for the page. | D | MVP |
| WSC-F-0052 | When data is available, the page shall display the Kp index to one decimal place. | Primary geomagnetic readout. | D | MVP |
| WSC-F-0053 | When data is available, the page shall display the Kp activity-level label defined in §3.3.4. | Human-readable severity. | D | MVP |
| WSC-F-0054 | When data is available, the page shall display a nine-segment aurora bar whose number of filled segments equals the Kp index truncated to a whole number and clamped to the range 0 through 9. | Visual Kp magnitude. | D | MVP |
| WSC-F-0055 | The aurora bar shall color each filled segment by its position: segments 0–2 green, segments 3–4 yellow, segments 5–6 orange, and segments 7–8 red. | Severity color coding consistent with §3.3.4. | I | MVP |
| WSC-F-0056 | When data is available, the page shall display the solar wind speed in whole kilometers per second. | Solar wind readout. | D | MVP |
| WSC-F-0057 | When data is available, the page shall display the latest solar flare class defined in §3.3.5. | X-ray flare activity. | D | MVP |
| WSC-F-0058 | The page shall apply a color theme modulated by the Kp index per the bands in §3.3.4. | Page re-tints with geomagnetic severity. | D | MVP |
| WSC-F-0059 | When data is available, the page shall display a geomagnetic hint line whose text is selected by the Kp index per the bands below. | Plain-language guidance on aurora visibility. | D | MVP |

The geomagnetic hint line (WSC-F-0059) maps the Kp index to one of three fixed strings:

| Kp range | Hint text |
|---|---|
| Kp ≥ 5 | `Aurora may be visible from N. US!` |
| 3 ≤ Kp < 5 | `Aurora visible at high latitudes` |
| Kp < 3 | `Quiet geomagnetic conditions` |

### §4.6 Sky Tonight Page — WSC-F-0060 through WSC-F-0066, 7 requirements

The Sky Tonight page renders the common header bar (WSC-F-0043). Its moon data is computed on-device and is always present; the page therefore has no soft-failure placeholder.

| ID | Requirement | Rationale | V | MVP |
|---|---|---|---|---|
| WSC-F-0060 | The Sky Tonight page shall draw a moon glyph whose illuminated shape corresponds to the current moon age. | Visual phase that matches the sky. | D | MVP |
| WSC-F-0061 | The page shall display the current moon phase name defined in §3.3.6. | Named phase readout. | D | MVP |
| WSC-F-0062 | The page shall display the moon illumination as a whole percentage. | Illuminated-fraction readout. | D | MVP |
| WSC-F-0063 | The page shall display the moon age in days to one decimal place. | Days-since-new-moon readout. | D | MVP |
| WSC-F-0064 | When weather data is available, the page shall display today's sunrise time. | Sun timing, sourced from the weather data. | D | MVP |
| WSC-F-0065 | When weather data is available, the page shall display today's sunset time. | Sun timing, sourced from the weather data. | D | MVP |
| WSC-F-0066 | The page shall compute moon age, illumination, and phase on-device without any network request. | No external dependency; data is always available. | A | MVP |

### §4.7 Next Launch Page — WSC-F-0067 through WSC-F-0076, 10 requirements

The Next Launch page renders the common header bar (WSC-F-0043). Its data fields follow.

| ID | Requirement | Rationale | V | MVP |
|---|---|---|---|---|
| WSC-F-0067 | When launch data is unavailable, the Next Launch page shall display the text "(no launch data)". | Soft-failure placeholder for the page. | D | MVP |
| WSC-F-0068 | When data is available, the page shall display the launch provider name. | Who is launching. | D | MVP |
| WSC-F-0069 | When data is available, the page shall display the launch vehicle name. | Which rocket. | D | MVP |
| WSC-F-0070 | When data is available, the page shall display the mission name. | What is launching. | D | MVP |
| WSC-F-0071 | When data is available, the page shall display a countdown to the launch net time, updating once per second. | Live time-to-launch. | D | MVP |
| WSC-F-0072 | Before the net time, the countdown shall be displayed as a T-minus value, including a whole-day field when one or more days remain. | T-minus format with optional day field. | D | MVP |
| WSC-F-0073 | After the net time has passed, the countdown shall be displayed as a T-plus value in hours, minutes, and seconds. | Post-liftoff elapsed time. | D | MVP |
| WSC-F-0074 | When data is available, the page shall display the launch pad or location, prefixed with "Pad: ". | Where it launches from. | D | MVP |
| WSC-F-0075 | When data is available, the page shall display the launch net time in local time as a date and time. | Absolute launch time for the user's zone. | D | MVP |
| WSC-F-0076 | The page shall truncate over-length text fields to fit the display, per the limits below. | Prevents text overflow on the 320 px-wide screen. | I | MVP |

Field truncation limits (WSC-F-0076):

| Field | Maximum characters |
|---|---|
| Provider | 25 |
| Vehicle | 25 |
| Mission name | 50 |
| Pad / location | 46 |

### §4.8 Data Acquisition and Refresh Scheduling — WSC-F-0077 through WSC-F-0091, 15 requirements

This subsection specifies how the background fetcher task (launched per WSC-F-0011) acquires data, schedules refreshes, and publishes results to the shared state read by the display loop.

| ID | Requirement | Rationale | V | MVP |
|---|---|---|---|---|
| WSC-F-0077 | The background fetcher task shall evaluate all refresh schedules once every 500 milliseconds. | Fixed polling cadence for the scheduler loop. | T | MVP |
| WSC-F-0078 | All network data acquisition shall occur on core 0 within the background fetcher task. | Keeps blocking I/O off the core-1 display loop. | A | MVP |
| WSC-F-0079 | The device shall refresh each data source only after its configured interval defined in §3.3.7 has elapsed since that source's last successful fetch. | Per-source cadence; bounds API call frequency. | T | MVP |
| WSC-F-0080 | Each HTTP or HTTPS request shall use a network timeout of 12 seconds. | Matches `API_TIMEOUT_MS`; bounds a stalled request. | T | MVP |
| WSC-F-0081 | The device shall publish fetched data to the shared state only when the fetch succeeds. | A failed fetch must not replace good data. | T | MVP |
| WSC-F-0082 | On a failed fetch, the device shall retain the last successfully fetched values for that source. | Soft-failure continuity; screen keeps showing last-good. | D | MVP |
| WSC-F-0083 | On a failed fetch, the device shall schedule the next attempt for that source 60 seconds later rather than after the full refresh interval. | Matches `FETCH_RETRY_AFTER_FAIL_SEC`; recovers from transient errors quickly. | T | MVP |
| WSC-F-0084 | The device shall zero-initialize each fetch's working data structure before the fetch. | Prevents publishing uninitialized values on partial fetches. | A | MVP |
| WSC-F-0085 | The device shall guard every read and write of the shared weather, ISS, space-weather, and launch data with a mutex. | Atomic hand-off between the core-0 fetcher and core-1 renderer. | A | MVP |
| WSC-F-0086 | The display loop shall snapshot the shared data under the mutex and render from the local copy without holding the mutex. | Rendering never blocks the fetcher and vice versa. | A | MVP |
| WSC-F-0087 | The ISS position fetch shall preserve the most recent people-in-space count. | The position endpoint does not carry crew count; it must not be cleared. | T | MVP |
| WSC-F-0088 | The people-in-space fetch shall update only the people-in-space count, leaving all other ISS fields unchanged. | Open Notify is flaky; its failure must not blank ISS position. | T | MVP |
| WSC-F-0089 | A space-weather refresh shall be treated as failed if the Kp-index sub-fetch fails. | Kp is the primary datum; without it the page has no valid data. | T | MVP |
| WSC-F-0090 | A space-weather refresh shall be treated as successful when the Kp-index sub-fetch succeeds even if the solar-wind or flare-class sub-fetch fails, leaving those fields at zero. | Partial space-weather data is still useful. | T | MVP |
| WSC-F-0091 | HTTPS requests shall be made without TLS certificate validation. | As-built `setInsecure()`; the device carries no certificate store. See §5. | I | MVP |

### §4.9 Soft-Failure and Resilience — WSC-F-0092 through WSC-F-0096, 5 requirements

This subsection states the system-wide reliability behaviors that the per-page and acquisition requirements implement.

| ID | Requirement | Rationale | V | MVP |
|---|---|---|---|---|
| WSC-F-0092 | The device shall not perform a software reset or reboot in response to any runtime failure. | Core "all failures are soft" principle; no `ESP.restart()` exists in the firmware. | I | MVP |
| WSC-F-0093 | The device shall continue operating the display and touch navigation when any or all data sources are unavailable. | A data outage must not freeze the UI; touch runs on core 1 independent of fetches. | D | MVP |
| WSC-F-0094 | Each data page that depends on a network source shall display a page-specific placeholder when that source's data is unavailable. | Communicates the outage without blanking the page; implemented by WSC-F-0040, 0044, 0051, 0067. | D | MVP |
| WSC-F-0095 | A malformed or unparseable API response shall be treated as a failed fetch. | Bad JSON must not corrupt the displayed data; triggers the last-good retention of WSC-F-0082. | T | MVP |
| WSC-F-0096 | An HTTP response with a status other than 200 shall be treated as a failed fetch. | Non-success responses must not be parsed as data. | T | MVP |

### §4.10 Theming — WSC-F-0097 through WSC-F-0099, 3 requirements

The device applies one of three theming modes per page, summarized below.

| Page | Theming mode | Selector | Specified by |
|---|---|---|---|
| Clock + Weather | Weather-driven (with day/night variants) | weather code + day/night | WSC-F-0039 (palettes §3.3.3) |
| ISS Live | Fixed space theme | — | WSC-F-0097 |
| Space Weather | Kp-modulated | Kp index | WSC-F-0058 (bands §3.3.4) |
| Sky Tonight | Fixed space theme | — | WSC-F-0097 |
| Next Launch | Fixed space theme | — | WSC-F-0097 |

| ID | Requirement | Rationale | V | MVP |
|---|---|---|---|---|
| WSC-F-0097 | The ISS Live, Sky Tonight, and Next Launch pages shall apply the fixed space color theme. | These pages do not vary their palette with data. | I | MVP |
| WSC-F-0098 | When a page's active theme changes, the device shall redraw that page's chrome. | A new palette requires repainting the background and header. | D | MVP |
| WSC-F-0099 | The device shall apply exactly one theme to each page according to that page's theming mode. | Governing rule tying the three theming modes together. | A | MVP |

---

## 5. Non-Functional Requirements

All requirements in §5 have status **Draft** and traceability **Derived** (descriptive of firmware v1.1.0). Verification methods follow the §1.5 abbreviations.

### §5.1 Performance — WSC-NF-0001 through WSC-NF-0004

| ID | Requirement | Rationale | V |
|---|---|---|---|
| WSC-NF-0001 | A navigation tap shall result in a page change within 250 milliseconds under any data-fetch condition. | Touch runs on core 1, independent of the core-0 fetcher; fetches never block input. | T |
| WSC-NF-0002 | The display shall refresh at least once per second. | One-second render tick keeps the clock and live countdown current. | T |
| WSC-NF-0003 | A page selected by a navigation tap shall complete its first full render within 500 milliseconds. | Page redraw is local with no network dependency. | D |
| WSC-NF-0004 | The boot sequence shall use only bounded blocking waits — WiFi association ≤ 30 s (WSC-F-0006), NTP acquisition ≤ 8 s (WSC-F-0009), and each initial fetch ≤ 12 s (WSC-F-0080) — so that startup cannot hang indefinitely. | Bounds worst-case startup; the device always reaches an interactive state. | A |

### §5.2 Reliability and Availability — WSC-NF-0005 through WSC-NF-0007

| ID | Requirement | Rationale | V |
|---|---|---|---|
| WSC-NF-0005 | The device shall operate continuously with no scheduled or failure-induced reboot. | Implements the soft-failure principle (WSC-F-0092). | A |
| WSC-NF-0006 | Following restoration of WiFi after a drop, the device shall re-establish connectivity within 60 seconds. | The background fetcher retries automatically each cycle (WSC-F-0012); each attempt is bounded by the 30 s association timeout (WSC-F-0006). | D |
| WSC-NF-0007 | The device shall continue display and touch operation while degrading to last-good or placeholder values on any data-source failure. | System-wide soft-failure behavior (§4.9). | D |

### §5.3 Security — WSC-NF-0008 through WSC-NF-0010

| ID | Requirement | Rationale | V |
|---|---|---|---|
| WSC-NF-0008 | WiFi credentials shall be supplied exclusively at build time through a git-ignored `secrets.h` file, with no path to enter or display them through any device interface. | Keeps credentials out of source control and off the screen. | I |
| WSC-NF-0009 | The device shall not transmit user credentials or personal data to any external service. | All external calls are read-only requests to public APIs; no PII is held or sent. | A |
| WSC-NF-0010 | HTTPS connections shall be established without TLS certificate validation. | As-built `setInsecure()` (WSC-F-0091); the device holds no certificate store and exchanges only public read-only data. This is an accepted limitation (see §11, OI-003). | I |

### §5.4 Resource and Hardware Constraints — WSC-NF-0011 through WSC-NF-0013

| ID | Requirement | Rationale | V |
|---|---|---|---|
| WSC-NF-0011 | The firmware image shall fit the "Huge APP (3MB No OTA)" partition scheme on the ESP32-2432S028R. | As-built board/partition configuration (N-8). | T |
| WSC-NF-0012 | The background fetcher task shall be allocated an 8192-byte stack. | Sized for HTTPClient plus ArduinoJson working memory. | I |
| WSC-NF-0013 | The display backlight shall be driven at a fixed PWM duty of 200 of 255. | Matches WSC-F-0002; no runtime brightness control. | I |

### §5.5 Maintainability and Portability — WSC-NF-0014 through WSC-NF-0016

| ID | Requirement | Rationale | V |
|---|---|---|---|
| WSC-NF-0014 | The device location and time zone shall be configurable at build time through the `LATITUDE`, `LONGITUDE`, and `LOCAL_TZ` constants. | Relocation requires only a recompile, no code change. | I |
| WSC-NF-0015 | The touch coordinate extents shall be configurable at build time through the `TOUCH_RAW_*_MIN`/`MAX` constants. | Per-unit touch calibration without code change. | I |
| WSC-NF-0016 | The firmware shall build under Arduino IDE 2.x using the libraries identified in §2. | Reproducible build toolchain. | T |

---

## 6. Data Model

### 6.1 Overview

WSC holds no persistent data. The data model consists of six in-RAM C structs. One instance of each data struct (`WeatherData`, `ISSData`, `SpaceWxData`, `LaunchData`, `SkyData`) exists in global state; the active `Theme` is a single working instance. The `weather`, `iss`, `spwx`, and `launch` instances are written by the core-0 fetcher and read by the core-1 renderer under a mutex (WSC-F-0085); `sky` and `theme` are accessed only on core 1. A `bool ok` flag (where present) is true only after a successful fetch; missing numeric values use `NAN` or a documented sentinel. The `error[40]` field holds a short diagnostic tag (for example `wifi_down`, `http_404`, `json:...`, `no_data`) when a fetch fails.

### 6.2 WeatherData

| Field | Type | Units / Constraints | Description |
|---|---|---|---|
| ok | bool | — | True after a successful weather fetch. |
| tempF | float | °F; `NAN` if absent | Current temperature. |
| feelsF | float | °F; `NAN` if absent | Apparent ("feels-like") temperature. |
| humidity | int | %; default 0 | Current relative humidity. |
| windMph | float | mph; default 0.0 | Wind speed; displayed on the Clock+Weather page (WSC-F-0100). |
| weatherCode | int | WMO code; −1 if absent | Current weather code (§3.3.2). |
| isDay | bool | — | True when Open-Meteo reports daytime. |
| todayHi | float | °F; `NAN` if absent | Today's high. |
| todayLo | float | °F; `NAN` if absent | Today's low. |
| tmrwHi | float | °F; `NAN` if absent | Tomorrow's high. |
| tmrwLo | float | °F; `NAN` if absent | Tomorrow's low. |
| tmrwCode | int | WMO code; −1 if absent | Tomorrow's weather code. |
| sunrise | char[12] | "h:MM AM/PM" or "--" | Today's sunrise, local 12-hour. |
| sunset | char[12] | "h:MM AM/PM" or "--" | Today's sunset, local 12-hour. |
| error | char[40] | — | Diagnostic tag on failure. |

### 6.3 ISSData

| Field | Type | Units / Constraints | Description |
|---|---|---|---|
| ok | bool | — | True after a successful ISS position fetch. |
| lat | float | degrees | ISS latitude. |
| lon | float | degrees | ISS longitude. |
| altKm | float | km | ISS altitude. |
| velKmH | float | km/h | ISS orbital velocity. |
| peopleInSpace | int | count; −1 = unknown | People currently in space (Open Notify). |
| distFromUserKm | float | km | Great-circle distance from the configured location, computed on-device. |
| visibility | char[16] | `daylight` / `eclipsed` / `?` | ISS visibility state (§3.1), passthrough of N-2. |
| error | char[40] | — | Diagnostic tag on failure. |

### 6.4 SpaceWxData

| Field | Type | Units / Constraints | Description |
|---|---|---|---|
| ok | bool | — | True when the Kp sub-fetch succeeded (WSC-F-0089). |
| kp | float | 0–9 | Planetary Kp index. |
| windSpeed | int | km/s; default 0 | Solar wind speed. |
| flareClass | char[8] | e.g. "M2.5"; §3.3.5 | Latest GOES X-ray flare class. |
| error | char[40] | — | Diagnostic tag on failure. |

### 6.5 LaunchData

| Field | Type | Units / Constraints | Description |
|---|---|---|---|
| ok | bool | — | True after a successful launch fetch. |
| name | char[64] | — | Mission name. |
| provider | char[40] | — | Launch service provider. |
| vehicle | char[40] | — | Rocket / vehicle name. |
| pad | char[48] | — | Launch pad or location. |
| netEpoch | time_t | UTC epoch seconds; 0 if unparseable | Launch net (no-earlier-than) time. |
| error | char[40] | — | Diagnostic tag on failure. |

### 6.6 SkyData

| Field | Type | Units / Constraints | Description |
|---|---|---|---|
| moonAge | float | 0–29.53 days | Days since the most recent new moon. |
| moonIllum | float | 0–100 % | Illuminated fraction. |
| moonPhase | char[20] | §3.3.6 phase name | Current moon phase name. |

`SkyData` has no `ok` field: it is always computed on-device (WSC-F-0066) and is never invalid.

### 6.7 Theme

| Field | Type | Units / Constraints | Description |
|---|---|---|---|
| bg | uint16_t | RGB565 | Page background color. |
| accent | uint16_t | RGB565 | Primary highlight color. |
| secondary | uint16_t | RGB565 | Secondary highlight color. |
| text | uint16_t | RGB565 | Primary text color. |
| dim | uint16_t | RGB565 | Subdued/label text color. |
| headerBg | uint16_t | RGB565 | Header bar background. |
| headerFg | uint16_t | RGB565 | Header bar foreground. |

The concrete palette values for every theme are listed in §8.8.

### 6.8 Relationships and Cardinality

Each struct is a singleton held in global state — there are no collections and no parent/child relationships. `WeatherData` is read by both the Clock+Weather page (§4.3) and the Sky Tonight page (§4.6, sunrise/sunset). The active `Theme` is derived per render from `WeatherData` (Clock page), `SpaceWxData.kp` (Space Weather page), or the fixed space palette (ISS/Sky/Launch), per §4.10. No struct is persisted; all are reinitialized on each boot.

---

## 7. Interface / API Specification

All external interfaces are outbound HTTP/HTTPS GET requests to keyless public APIs; the device exposes no inbound interface. This section specifies each request and the response fields the firmware consumes.

### 7.1 Common Interface Conventions

Requirements in §7 use TYPE **IF** and carry status **Draft**, traceability **Derived**.

- Every request is an HTTP GET with no request body and no API key.
- HTTPS requests are made without TLS certificate validation (WSC-NF-0010); the Open Notify endpoint is plain HTTP (it offers no HTTPS).
- Each request uses a 12-second timeout (WSC-F-0080). A non-200 status (WSC-F-0096) or an unparseable body (WSC-F-0095) is treated as a failed fetch and is not published.
- Numeric/string defaults on missing JSON keys follow §6 (e.g., `NAN`, −1, 0, "?").

| ID | Requirement | Rationale | V |
|---|---|---|---|
| WSC-IF-0001 | The device shall request Open-Meteo forecast data with temperature in Fahrenheit, wind speed in mph, automatic time zone, and a two-day forecast horizon. | Fixes the units and horizon the display logic assumes. | I |
| WSC-IF-0002 | The device shall request ISS position data for NORAD catalog object 25544. | Identifies the tracked satellite. | I |
| WSC-IF-0003 | The device shall send the HTTP header `User-Agent: CYD-clock/1.0` on Launch Library 2 requests. | The provider requires a User-Agent; requests without one may be rejected. | I |
| WSC-IF-0004 | The device shall request the people-in-space count over plain HTTP. | Open Notify provides no HTTPS endpoint. | I |

### 7.2 Open-Meteo Forecast (N-1)

**GET** `https://api.open-meteo.com/v1/forecast`

| Query parameter | Value |
|---|---|
| latitude / longitude | `LATITUDE` / `LONGITUDE` (4 decimals) |
| current | `temperature_2m, relative_humidity_2m, weather_code, wind_speed_10m, is_day, apparent_temperature` |
| daily | `temperature_2m_max, temperature_2m_min, weather_code, sunrise, sunset` |
| timezone | `auto` |
| temperature_unit | `fahrenheit` |
| wind_speed_unit | `mph` |
| forecast_days | `2` |

Response fields consumed: `current.temperature_2m`, `current.apparent_temperature`, `current.relative_humidity_2m`, `current.wind_speed_10m`, `current.weather_code`, `current.is_day`; `daily.temperature_2m_max[0]`/`[1]`, `daily.temperature_2m_min[0]`/`[1]`, `daily.weather_code[1]`, `daily.sunrise[0]`, `daily.sunset[0]`. A null `current` or `daily` object is treated as a failed fetch ("no_data").

### 7.3 WhereTheISS.at (N-2)

**GET** `https://api.wheretheiss.at/v1/satellites/25544`

Response fields consumed: `latitude`, `longitude`, `altitude`, `velocity`, `visibility`. The device computes `distFromUserKm` locally (haversine from `LATITUDE`/`LONGITUDE`); it is not part of the response.

### 7.4 Open Notify — Astros (N-3)

**GET** `http://api.open-notify.org/astros.json` (plain HTTP)

Response field consumed: `number` (people in space). This fetch updates only the people-in-space count and leaves all other ISS fields unchanged (WSC-F-0088); on failure the previous count is retained (WSC-F-0082); a missing value yields −1.

### 7.5 NOAA SWPC (N-4)

Three independent GET requests; the Kp request governs page validity (WSC-F-0089), the other two are best-effort (WSC-F-0090).

| # | Endpoint | Field consumed |
|---|---|---|
| 1 | `https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json` | Last row, index 1 → `kp` (array-of-arrays; row 0 is the header) |
| 2 | `https://services.swpc.noaa.gov/products/solar-wind/plasma-1-day.json` | Last row, index 2 → `windSpeed` |
| 3 | `https://services.swpc.noaa.gov/json/goes/primary/xrays-1-day.json` | Among the last 24 samples, the maximum `flux` where `energy` contains "0.1-0.8" → `flareClass` (§3.3.5) |

### 7.6 Launch Library 2 (N-5)

**GET** `https://ll.thespacedevs.com/2.2.0/launch/upcoming/?limit=1` with header `User-Agent: CYD-clock/1.0`

Response fields consumed from `results[0]`: `name`; `launch_service_provider.name`; `rocket.configuration.name` (falling back to `rocket.full_name`); `pad.name` (falling back to `pad.location.name`); `net` (ISO 8601, parsed on-device to a UTC epoch). An empty `results` array is treated as a failed fetch ("no_launches").

### 7.7 NTP (N-6)

Time is synchronized with `configTzTime` using the POSIX `LOCAL_TZ` string and three servers: `pool.ntp.org`, `time.nist.gov`, `time.google.com`. This is an SNTP background service, not an HTTP request; the initial acquisition waits up to 8 seconds (WSC-F-0009).

---

## 8. User Interface Specification

The UI is rendered directly to a 2.8-inch TFT via TFT_eSPI; there is no web or templated UI. All views are full-screen and selected by touch (§4.2). Requirements in §8 use TYPE **UI**, status **Draft**, traceability **Derived**.

### 8.1 Display Geometry and Conventions

The screen is 320 × 240 pixels in landscape orientation (rotation 1). The five data pages share a fixed vertical grid: a 20-pixel top band (`y` 0–19), a content area (`y` 20–217), and a 22-pixel navigation bar at the bottom (`y` 218–239). On the four space-data pages the top band is the common header bar (WSC-UI-0004 / WSC-F-0043); on the Clock+Weather page the same band carries the date at the left and the WiFi RSSI at the right (§8.3). Colors are 16-bit RGB565.

| ID | Requirement | Rationale | V |
|---|---|---|---|
| WSC-UI-0001 | The display shall operate at 320 × 240 pixels in landscape orientation (rotation 1). | As-built panel geometry. | I |
| WSC-UI-0002 | Each of the five data pages shall reserve the bottom 22 pixels for the navigation bar. | Persistent navigation affordance. | I |
| WSC-UI-0003 | The navigation bar shall display a left arrow, a right arrow, the centered current-page label, and five position dots with the current page's dot filled. | Orientation and position feedback. | I |
| WSC-UI-0004 | The four space-data pages shall reserve the top 20 pixels for a header bar containing the page title at the left and the WiFi RSSI at the right. | Common chrome (WSC-F-0043). | I |
| WSC-UI-0005 | The UI shall not wrap text; any string exceeding its field shall be truncated to fit (WSC-F-0076). | Prevents overflow into adjacent fields. | I |
| WSC-UI-0006 | The UI shall redraw only fields whose content changed since the previous render, performing a full repaint only on a page change or theme change. | Flicker-free updates and render efficiency. | A |
| WSC-UI-0007 | A string truncated to fit its field shall end with a `>` marker. | Marks truncation so the operator knows the value was clipped. | I |

### 8.2 Transient and System Views

These full-screen views appear during startup or calibration and are not part of the page-cycle.

| View | Trigger | Elements |
|---|---|---|
| Boot progress | Startup (WSC-F-0004) | Title "Clock + Sky + Space"; a progress bar filled to 20/50/80/100 %; a status label ("Connecting WiFi...", "Syncing time...", "Fetching data...", "Ready"). |
| Connecting | During WiFi association | "Connecting to WiFi..."; the SSID; "Attempt N" once retries begin. |
| Error | Initial WiFi failure (WSC-F-0007) | Red title (e.g., "WIFI FAILED"); a detail line ("Will keep retrying"). |
| Touch calibration | Built with calibration enabled (WSC-F-0023) | On-screen instructions to open Serial Monitor and tap each corner to record raw extents. |

### 8.3 Clock + Weather View (page 0)

Navigation: left-half tap → Next Launch (wrap); right-half tap → ISS Live. Theme per §4.10 (weather-driven).

| Element | Location | Content / source |
|---|---|---|
| Date | top-left header | Weekday, month, day, year (WSC-F-0027) |
| RSSI | top-right header | WiFi dBm (WSC-F-0028) |
| Clock | center, text size 6 | 12-hour time with blinking colon (WSC-F-0025/0026) |
| AM/PM | below clock | AM or PM (WSC-F-0025) |
| Weather icon | left of mid-band | Animated icon (WSC-F-0036) |
| Temperature | mid-band | Current °F (WSC-F-0029) |
| Condition / feels / humidity + wind | right column | Label, feels-like, RH and wind on one line (WSC-F-0030/0031/0032/0100) |
| Today Hi/Lo, sunrise, sunset | lower-left | WSC-F-0033/0034/0035 |
| Tomorrow block | lower-right | Label + Hi/Lo (WSC-F-0037/0038) |
| Weather placeholder | mid-band (no data) | "(loading weather...)" (WSC-F-0040) |

### 8.4 ISS Live View (page 1)

Navigation: left → Clock; right → Space Weather. Fixed space theme.

| Element | Location | Content / source |
|---|---|---|
| Lat / lon | upper area, text size 3 | "lat, lon" to 2 dp (WSC-F-0045) |
| Altitude | mid-left | km (WSC-F-0046) |
| Velocity | mid-right | km/h (WSC-F-0047) |
| Distance | mid-band | "… km from you" (WSC-F-0048) |
| Crew + visibility | lower | count + visibility, or visibility only (WSC-F-0049/0050) |
| Placeholder | content area (no data) | "(no ISS data)" (WSC-F-0044) |

### 8.5 Space Weather View (page 2)

Navigation: left → ISS; right → Sky Tonight. Kp-modulated theme (§4.10).

| Element | Location | Content / source |
|---|---|---|
| Kp index | upper-left, text size 7 | Kp to 1 dp (WSC-F-0052) |
| Activity level | upper-right, text size 3 | QUIET/ACTIVE/AURORA/STORM (WSC-F-0053) |
| Aurora bar | mid-band | 9 segments, colored by level (WSC-F-0054/0055) |
| Solar wind | lower | km/s (WSC-F-0056) |
| Flare class | lower | e.g. "M2.5" (WSC-F-0057) |
| Hint line | bottom of content | Kp-band hint string (WSC-F-0059) |
| Placeholder | content area (no data) | "(no data)" (WSC-F-0051) |

### 8.6 Sky Tonight View (page 3)

Navigation: left → Space Weather; right → Next Launch. Fixed space theme.

| Element | Location | Content / source |
|---|---|---|
| Moon glyph | left, ~96 px | Drawn to match moon age (WSC-F-0060) |
| Phase name | right, text size 2 | §3.3.6 name (WSC-F-0061) |
| Illumination | right | whole % (WSC-F-0062) |
| Age | right | days, 1 dp (WSC-F-0063) |
| Sunrise / sunset | lower | from weather data (WSC-F-0064/0065) |

### 8.7 Next Launch View (page 4)

Navigation: left → Sky Tonight; right → Clock (wrap). Fixed space theme.

| Element | Location | Content / source |
|---|---|---|
| Provider | upper, text size 2 | WSC-F-0068 (≤25 chars) |
| Vehicle | below provider | WSC-F-0069 (≤25 chars) |
| Mission | below vehicle, size 1 | WSC-F-0070 (≤50 chars) |
| Countdown | mid-band, text size 4 | live T- / T+ (WSC-F-0071/0072/0073) |
| Pad | lower | "Pad: …" (WSC-F-0074, ≤46 chars) |
| Local launch time | lower | date + time (WSC-F-0075) |
| Placeholder | content area (no data) | "(no launch data)" (WSC-F-0067) |

### 8.8 Theme Palette Values (RGB565)

All themes use `text` = `0xFFFF` and `dim` = `0x738E` unless overridden below.

**Clock+Weather themes (§3.3.3):**

| Theme | bg | accent | secondary | headerBg | headerFg |
|---|---|---|---|---|---|
| Clear — day | 0x0084 | 0xFEA0 | 0x07FF | 0x0146 | 0xFEA0 |
| Clear — night | 0x0009 | 0xC618 | 0x041F | 0x0010 | 0xC618 |
| Cloudy — day | 0x10A4 | 0xFEC0 | 0xBDF7 | 0x1965 | 0xFEC0 |
| Cloudy — night | 0x0009 | 0xC618 | 0x528A | 0x0010 | 0xC618 |
| Fog (also default) | 0x1082 | 0xBDF7 | 0x738E | 0x18C3 | 0xBDF7 |
| Rain | 0x0010 | 0x07FF | 0x041F | 0x0017 | 0x07FF |
| Snow | 0x0009 | 0xFFFF | 0xAEDB | 0x0011 | 0xAEDB |
| Thunderstorm | 0x1003 | 0xFFE0 | 0xA01F | 0x1804 | 0xFFE0 |

**Fixed space theme (ISS / Sky / Launch):** bg `0x0009`, accent `0x07FF`, secondary `0xC618`, headerBg `0x0010`, headerFg `0x07FF`.

**Space Weather Kp-modulated overrides** (applied over the fixed space theme, §3.3.4):

| Kp band | accent | headerBg | headerFg |
|---|---|---|---|
| Kp < 3 | 0x07E0 | 0x0260 | 0xFFFF |
| 3 ≤ Kp < 5 | 0xFFE0 | 0x4960 | 0x0000 |
| 5 ≤ Kp < 7 | 0xFD00 | 0x4980 | 0xFFFF |
| Kp ≥ 7 | 0xF800 | 0x4000 | 0xFFFF |

---

## 9. Development Plan

This specification documents released firmware. Phase 1 is complete; there is no committed further scope.

| Phase | Scope | Deliverables | Acceptance Criteria | MVP? |
|---|---|---|---|---|
| **Phase 1 — Released (v1.1.0)** | Full firmware: startup and connectivity, touch navigation, the five pages (including wind speed on the Clock+Weather page), data acquisition and refresh scheduling, soft-failure handling, and theming. Baseline v1.0.0 plus the OI-004 wind-display feature and `bz` removal; TLS-insecure accepted (OI-003). | Firmware flashed to the ESP32-2432S028R, tagged `v1.1.0`. | All functional requirements WSC-F-0001 through WSC-F-0099 and WSC-F-0100, all non-functional WSC-NF-0001 through WSC-NF-0016, all interface WSC-IF-0001 through WSC-IF-0004, and all UI WSC-UI-0001 through WSC-UI-0007 verified on hardware by their assigned methods (§1.5). | MVP — complete |

Phase 1 is verified by the firmware in production at tag `v1.1.0`; every Phase 1 requirement is tagged MVP. The open issues that were candidate enhancements (OI-003, OI-004) have been resolved into this build (§11); no further phases are committed. Future enhancements would be added as new requirements and this plan updated.

---

## 10. Assumptions and Constraints

### 10.1 Assumptions

| ID | Assumption |
|---|---|
| A-1 | A 2.4 GHz WiFi network with internet access is reachable, and its credentials are supplied at build time in `secrets.h`. |
| A-2 | The device operates as a single-user desk display; there is no concurrent-user scenario. |
| A-3 | The public APIs in §2 remain available and retain the response fields the firmware consumes (§7); a provider schema change may require a firmware update. |
| A-4 | The device serves one fixed location and time zone per build, set by `LATITUDE`, `LONGITUDE`, and `LOCAL_TZ`. |
| A-5 | At least one configured NTP server is reachable for wall-clock accuracy. |

### 10.2 Constraints

| ID | Constraint |
|---|---|
| C-1 | The target hardware is the ESP32-2432S028R (2USB revision); the touch controller is on a dedicated VSPI bus (WSC-F-0003). |
| C-2 | The firmware fits and runs under the "Huge APP (3MB No OTA)" partition scheme; over-the-air update is not available (WSC-NF-0011). |
| C-3 | All layouts are fixed to the 320 × 240 display geometry (WSC-UI-0001). |
| C-4 | HTTPS connections perform no TLS certificate validation (WSC-NF-0010; OI-003). |
| C-5 | The device requires a stable 5 V supply; marginal USB power can brown out the device during radio current spikes — a wall charger and a substantial USB cable are required. |
| C-6 | Location, time zone, WiFi credentials, and touch calibration are compile-time constants; changing any of them requires reflashing the firmware (WSC-NF-0008, WSC-NF-0014, WSC-NF-0015). |
| C-7 | The device holds no persistent storage; all data is lost on power cycle and re-fetched on boot (§6.1). |

---

## 11. Open Issues

| OI ID | Description | Owner | Status | Resolution |
|---|---|---|---|---|
| OI-001 | First-frame behavior when the one-time initial fetch (WSC-F-0010) fails before any last-good value exists. | James | Resolved | The Clock+Weather page renders "(loading weather...)" and still shows the clock/date; day/night theme falls back to the local hour (WSC-F-0040/0041). |
| OI-002 | "ISS visibility state" (WSC-F-0049/0050) lacked a definition and value enumeration. | James | Resolved | Defined in §3.1 as a verbatim passthrough of the WhereTheISS.at field — `daylight`/`eclipsed`, or `?` when absent. |
| OI-003 | HTTPS uses no TLS certificate validation (WSC-F-0091 / WSC-NF-0010). | James | Resolved | Accepted as a v1.1.0 limitation: the device holds no certificate store and exchanges only read-only public data. No code change; retained as the documented posture in §5.3 and §10 (C-4). |
| OI-004 | Two unused struct fields: `WeatherData.windMph` is populated but displayed on no page; `SpaceWxData.bz` is declared but never populated. | James | Resolved | v1.1.0: wind speed is now displayed on the Clock+Weather page (WSC-F-0100, §6.2); the unused `bz` field was removed from `SpaceWxData` (§6.4). |
