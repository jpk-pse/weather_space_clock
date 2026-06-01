# Weather + Space Clock

A 5-page touchscreen desk display on the **ESP32-2432S028R** ("Cheap Yellow
Display"). Tap the lower-left or lower-right of the screen to cycle pages.

Originally forked from [nimnim111/Claude-Monitor](https://github.com/nimnim111/Claude-Monitor)
and rewritten end-to-end as a weather + space dashboard.

---

## Pages

| # | Page | Data |
|---|---|---|
| 0 | **Clock + Weather** | Big clock, current temp + conditions, animated icon, today's hi/lo, sun rise/set, tomorrow forecast |
| 1 | **ISS Live** | Current ISS lat/lon, altitude, velocity, distance from your location, # people in space |
| 2 | **Space Weather** | Kp index (color-coded), aurora forecast bar, solar wind speed, latest solar flare class |
| 3 | **Sky Tonight** | Moon phase (drawn glyph matches reality), illumination %, age in days, sun rise/set |
| 4 | **Next Launch** | Provider, vehicle, mission name, live T- countdown, pad location |

All five pages share a touchable bottom nav bar showing the current page name and dots for position.

The clock-and-weather page also re-tints its whole color theme based on the
current weather code — sunny / partly cloudy / rainy / snowy / fog / thunderstorm
/ clear-night each have a distinct palette.

---

## Hardware

| Part | Approx. cost | Notes |
|---|---|---|
| **ESP32-2432S028R** (CYD, 2USB rev) | ~$17 | The 2USB revision is the newer one with both USB-C and micro-USB. Touch is on a separate VSPI bus. |
| Short USB-C cable + wall charger | — | A 5V/2A+ wall charger is recommended over laptop USB — the radio current spike can brown out marginal USB ports. |
| Optional: 3D-printed case | — | See [Printables](https://www.printables.com/) — search "CYD 2.8 case". |

---

## APIs used (all free, no API key required)

| Source | Used for |
|---|---|
| [Open-Meteo](https://open-meteo.com/) | Weather, forecast, sunrise/sunset |
| [WhereTheISS.at](https://wheretheiss.at/) | ISS position, altitude, velocity |
| [Open Notify](http://open-notify.org/) | People currently in space (HTTP only, occasionally flaky) |
| [NOAA SWPC](https://www.swpc.noaa.gov/) | Kp index, solar wind, X-ray flares |
| [Launch Library 2](https://thespacedevs.com/llapi) | Next orbital launch worldwide |
| `pool.ntp.org` / `time.nist.gov` | Wall clock |

---

## Setup

### 1. Install Arduino IDE 2.x
Download from [arduino.cc](https://www.arduino.cc/en/software).

### 2. Add ESP32 board support
**File → Preferences →** Additional boards manager URLs:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```
Then **Tools → Board → Boards Manager →** install **esp32** by Espressif.

### 3. Install libraries
**Sketch → Include Library → Manage Libraries →** install:
- **TFT_eSPI** by Bodmer
- **ArduinoJson** by Benoit Blanchon (v7.x)
- **XPT2046_Touchscreen** by Paul Stoffregen

### 4. Configure TFT_eSPI for the CYD
Copy `User_Setup.h` from this repo into your TFT_eSPI library folder, replacing
the existing file:
```
Windows: Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
Linux/Mac: ~/Arduino/libraries/TFT_eSPI/User_Setup.h
```

### 5. Set up credentials
Copy `secrets.h.example` to `secrets.h` and fill in your WiFi info:
```cpp
#define WIFI_SSID  "YourNetworkName"
#define WIFI_PASS  "YourPassword"
```
`secrets.h` is gitignored — it won't be committed.

### 6. Set your location
In `weather_space_clock.ino`, update:
```cpp
#define LATITUDE   35.8109     // your latitude
#define LONGITUDE -84.2333     // your longitude
#define LOCAL_TZ   "EST5EDT,M3.2.0,M11.1.0"   // POSIX TZ string
```
POSIX TZ string examples for the US:
- Eastern: `EST5EDT,M3.2.0,M11.1.0`
- Central: `CST6CDT,M3.2.0,M11.1.0`
- Mountain: `MST7MDT,M3.2.0,M11.1.0`
- Pacific: `PST8PDT,M3.2.0,M11.1.0`

### 7. Calibrate touch (one-time)
At the top of `weather_space_clock.ino`, set:
```cpp
#define CALIBRATE_TOUCH 1
```
Upload. Open Serial Monitor at 115200. Tap each corner of the screen and watch
the `[touch raw] x=… y=… z=…` lines. Record the smallest/largest x and y values
seen, then paste them into the `TOUCH_RAW_*_MIN/MAX` defines below. Set
`CALIBRATE_TOUCH` back to `0`. Re-upload.

### 8. Board settings & upload
- **Tools → Board** → ESP32 Dev Module
- **Tools → Partition Scheme** → Huge APP (3MB No OTA)
- **Tools → Port** → whichever COM port the CYD enumerated as
- Click **Upload**

On boot you'll see a progress bar, then the clock + weather page. Tap the left
or right half of the screen to cycle pages.

---

## File structure

```
weather_space_clock/
├── weather_space_clock.ino   Main: setup, loop, structs, touch, page nav
├── ui.ino                    All page drawers, navbar, themes, animated icons
├── weather.ino               Open-Meteo fetcher
├── space.ino                 ISS + astros + NOAA SWPC + Launch Library fetchers
├── sky.ino                   Moon phase math (no network)
├── User_Setup.h              TFT_eSPI pin config for the CYD
├── secrets.h.example         Template — copy to secrets.h locally
└── README.md
```

---

## Notes

- **Touch on 2USB CYDs** is on a separate VSPI bus (XPT2046 controller).
  TFT_eSPI's built-in `getTouch()` won't work — `XPT2046_Touchscreen` library
  is used instead with its own SPI instance.
- **All failures are soft** — no `ESP.restart()` calls anywhere. WiFi drops
  reconnect in the loop, API failures show "(loading…)" or last good values,
  the device never reboots itself.
- **Refresh cadence** is tuned to minimize main-loop blocking for touch
  responsiveness: weather every 15 min, ISS every 30 s, space weather hourly,
  next launch hourly. ISS in particular was reduced from 8 s because the
  blocking HTTP call was eating touch events.
- **Brownout** has historically been a problem on CYDs with marginal USB
  power. Use a wall charger and a thick USB cable, not a laptop port.
