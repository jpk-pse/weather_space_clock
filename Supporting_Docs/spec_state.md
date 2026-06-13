# Spec State — WSC (Weather + Space Clock)

## Summary Block
- **Tool / abbreviation:** Weather + Space Clock / **WSC**
- **Spec revision:** Rev 1-draft (in progress)
- **Nature:** Descriptive spec of as-built firmware v1.0.0 (ESP32-2432S028R "CYD", Arduino/C++)
- **Current phase:** Phase 2 — incremental drafting
- **Sections confirmed:** §1–§11 ALL COMPLETE. Phase 3 consistency audit PASSED.
- **OI resolutions actioned (code + spec):** OI-004 → wind now displayed on Clock+Weather (WSC-F-0100; ui.ino humid line + buffers); unused SpaceWxData.bz removed (weather_space_clock.ino). OI-003 → TLS-insecure accepted (no code change). All 4 OIs now Resolved.
- **Firmware version bumped to 1.1.0** (minor: wind-display feature). Spec target/revision/§4-§5 preambles/§9/§2 updated to 1.1.0. User-Agent left "CYD-clock/1.0" (fixed string, not semver).
- **Spec ID inventory now:** F-0001..0100 (100), NF-0001..0016, IF-0001..0004, UI-0001..0007.
- **Status:** Rev 1-draft complete incl. OI fixes, pending James review → commit → release+docx.
- **Next:** commit (branch spec/wsc); firmware tag v1.1.0 at release; docx only on release.
- ID inventory: F-0001..0099 (99), NF-0001..0016 (16), IF-0001..0004 (4), UI-0001..0007 (7). OIs: 001 resolved, 002 resolved, 003 open (TLS), 004 open (windMph/bz unused).
- §2 endpoint versions CONFIRMED via fetchers. Geometry: SCREEN 320x240, ROT 1, header 20px (y0-19), content (y20-217), nav 22px (y218-239), PWM 200. clampStr truncation marker='>'. RGB565 palettes → put in §8.8; fix §3.3.3 & §6.7 refs.
- Weather API params (for §7): timezone=auto, temperature_unit=fahrenheit, wind_speed_unit=mph, forecast_days=2; current=temp/RH/code/wind/is_day/apparent; daily=tmax/tmin/code/sunrise/sunset.
- As-built dead fields: WeatherData.windMph populated(mph) but NOT displayed; SpaceWxData.bz declared, NEVER populated. → OI-004.
- §4 completeness vs intake Q12: verified, no gaps.
- REMAINING: §5 NF, §6 Data Model, §7 Interface/API, §8 UI, §9 Dev Plan, §10 Assumptions/Constraints, §11 Open Issues, appendices. Then Phase 3 final review.
- TODO §8/§10: capture screen geometry (SCREEN_W=320, SCREEN_H=240, ROT=1, NAV_H=22, header 20px) normatively.
- NOTE: F-0043 = common header bar (title + RSSI) for the 4 space pages; §4.5–4.7 reference it (no per-page RSSI repeat).
- §3.1 added "ISS visibility state" def (daylight/eclipsed/? passthrough of N-2). OI-002 resolved.
- **MVP policy:** v1.0.0 ships all features → all functional requirements tagged MVP (shipped); future enhancements → OIs
- **ID format:** `WSC-[TYPE]-[NNNN]` (TYPE = F / NF / UI / IF)

## Locked Scope Constraints (from Phase 1 intake)
- Single role: device owner/operator. No multi-user, no auth, no on-device config UI.
- Platform: ESP32-2432S028R (2USB rev), Arduino IDE 2.x, ESP32 board package, bare-metal.
- Stack: C++/`.ino`, TFT_eSPI, ArduinoJson v7, XPT2046_Touchscreen (touch on separate VSPI bus).
- APIs (free, no key): Open-Meteo, WhereTheISS.at, Open Notify, NOAA SWPC, Launch Library 2; NTP (pool.ntp.org / time.nist.gov).
- 5 pages: Clock+Weather, ISS Live, Space Weather, Sky Tonight, Next Launch + touchable bottom nav.
- Weather-code-driven color theming on Clock+Weather page.
- Soft-failure model: no `ESP.restart()`; failures → "(loading…)" or last-good values.
- One-time touch calibration workflow (CALIBRATE_TOUCH).
- WiFi creds in gitignored `secrets.h`.
- Out of scope: OTA, web/config UI, multi-device sync, persistence/history.

## Performance Thresholds (agreed — for §5) — REVISED for dual-core architecture
- Architecture: fetcherTask pinned core 0 (all HTTP); display/touch loop() core 1; mutex-guarded shared structs. Fetches NEVER block render.
- Touch-to-page-change latency: ≤ 250 ms, independent of fetch activity (250 ms debounce on core 1).
- Display refresh: at least once per second (1 Hz tick + immediate redraw on touch / forceRedraw).
- Page render: within 500 ms of selection.
- Cold boot to interactive Clock+Weather: target ~15 s under normal WiFi; restate vs worst case 30 s WiFi timeout + 8 s NTP at §5.
- WiFi recovery: reconnect within 60 s, no reboot.
- Uptime: indefinite, no self-reboot; all failures soft-degrade.
- Refresh cadence (fixed): weather 900 s, ISS 30 s, astros 3600 s, space-wx 3600 s, launch 3600 s; retry-after-fail 60 s.
- RETIRED (wrong single-core assumption): "max main-loop block per fetch 2 s", "worst-case touch lag during fetch 2.5 s". See debrief 2026-06-12.

## Section Decisions Log
(append one line per confirmed section)
- §1 Introduction confirmed 2026-06-12 — doc ID WSC-SPEC-001 (distinct from req IDs); TYPE set {F,NF,UI,IF}; NNNN 0001–9999; status tags MVP/Post-MVP; scope/out-of-scope locked per intake.
- §2 Applicable Documents confirmed 2026-06-12 — Normative N-1..N-12 (5 APIs, NTP, CYD HW, esp32 core, Arduino IDE 2.x, TFT_eSPI, ArduinoJson v7, XPT2046); Informative I-1..I-3. Sky Tonight = no external ref (on-device math). N-6 NTP updated to 3 servers incl time.google.com. TODO: verify endpoint versions vs space.ino/weather.ino at §7.
- §3 Definitions confirmed 2026-06-12 — 10 defs, 15 abbreviations; enumerations 3.3.1 pages(5), 3.3.2 weather labels(WMO), 3.3.3 themes (default=Fog @ui.ino:39), 3.3.4 Kp QUIET/ACTIVE/AURORA/STORM, 3.3.5 flare A/B/C/M/X, 3.3.6 moon 8 phases, 3.3.7 refresh cadence consts. RGB565 values deferred to §8/appendix.
- §4.1 System Startup & Connectivity confirmed 2026-06-12 — WSC-F-0001..0013 (13 reqs). Boot 20/50/80/100%; WiFi STA 30s timeout; NTP 8s; PWM 200/255; touch on VSPI; dual-core (fetcher core 0, display core 1, mutex). OI-001 opened (first-frame behavior if initial fetch fails). §4 preamble: all Draft/Derived/MVP.
- §4.2 Navigation & Touch Input confirmed 2026-06-12 — WSC-F-0014..0024 (11 reqs). Full left/right half touch zones, cyclic wrap, 250ms debounce, single change per press, calibration build mode (serial @115200, no nav). Midpoint→next. §3.1 nav-bar def corrected (MY ERROR logged).
- §4.3 Clock+Weather confirmed 2026-06-12 — WSC-F-0025..0042 (18 reqs). Clock 12h+AM/PM, colon blink 1Hz, date, RSSI; weather block conditional; whole °F; "(loading weather...)"; day=06:00–18:59; NaN→"--". OI-001 resolved.
- §4.4 ISS Live confirmed 2026-06-12 — WSC-F-0043..0050 (8 reqs). F-0043 common header (4 space pages). lat/lon 2dp, alt km, vel km/h, dist km (haversine), people+visibility / visibility-only. OI-002 resolved.
- §4.5 Space Weather confirmed 2026-06-12 — WSC-F-0051..0059 (9). "(no data)"; Kp 1dp; level §3.3.4; 9-seg aurora bar; wind km/s; flare §3.3.5; Kp theme; hint line (3 strings).
- §4.6 Sky Tonight confirmed 2026-06-12 — WSC-F-0060..0066 (7). Moon glyph, phase name §3.3.6, illum %, age 1dp, sun rise/set (from weather), on-device compute (no placeholder).
- §4.7 Next Launch confirmed 2026-06-12 — WSC-F-0067..0076 (10). "(no launch data)"; provider/vehicle/mission; live T-/T+ countdown; pad; local net time; truncation limits 25/25/50/46.
- §4.8 Data Acquisition confirmed 2026-06-12 — WSC-F-0077..0091 (15). 500ms cycle, core0, §3.3.7 intervals, 12s timeout, publish-on-success, last-good, 60s retry, zero-init, mutex, snapshot-render, ISS people preserve, astros count-only, spacewx Kp-required/wind+flare-tolerant, HTTPS no-TLS-verify.
- §4.9 Soft-Failure confirmed 2026-06-12 — WSC-F-0092..0096 (5). No reboot; keep UI live; per-page placeholder (governs F-0040/0044/0051/0067); malformed→fail; non-200→fail.
- §4.10 Theming confirmed 2026-06-12 — WSC-F-0097..0099 (3). Fixed space theme ISS/Sky/Launch; theme change→chrome redraw; one theme per page. (Clock=F-0039, SpaceWx=F-0058.)
