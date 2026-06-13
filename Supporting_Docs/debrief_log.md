# Debrief Log — WSC (Weather + Space Clock)

Append-only running record per `always_load.md` §2. Never rewrite past entries.

Format:
```
YYYY-MM-DD | [TAG] | <one-line description>
Detail: <one to three sentences>
```
Tags: [DECISION] [SPEC CHANGE] [MY ERROR] [LATE EDGE CASE] [ASSUMPTION FAIL] [SESSION]

---

2026-06-12 | [DECISION] | Spec WSC initiated as descriptive documentation of as-built v1.0.0 firmware.
Detail: No prior spec existed. Scope confirmed to reverse-document the shipped tool; all functional requirements tagged MVP (shipped), future enhancements logged as OIs.

2026-06-12 | [ASSUMPTION FAIL] | Assumed single-core blocking-loop model; v1.0.0 is actually dual-core with a background fetcher task.
Detail: During §5 performance-threshold discussion I assumed all API fetches block the main render/touch loop (per the README's note that ISS cadence was reduced because blocking HTTP ate touch events), and on that basis proposed "max main-loop block per fetch = 2 s" and "worst-case touch lag during fetch = 2.5 s." Reading weather_space_clock.ino revealed the shipped firmware pins a fetcherTask to core 0 for all HTTP while the display/touch loop runs on core 1 with a mutex, so fetches never block rendering. Those two §5 thresholds must be re-framed against the dual-core architecture before §5 is drafted.

2026-06-12 | [DECISION] | §5 performance thresholds re-framed for the dual-core architecture.
Detail: The two fetch-blocking thresholds were replaced because fetches run on core 0 and never block the core-1 render loop. New thresholds: display refreshes at least once per second (1 Hz tick + immediate redraw on touch); touch-to-page-change latency ≤ 250 ms independent of fetch activity. Boot ≤15 s to be restated against the 30 s WiFi + 8 s NTP worst case when §5 is drafted.

2026-06-12 | [DECISION] | Touch zones documented as full left/right screen halves, not the README's "lower-left/lower-right corners."
Detail: handleTouch() uses sx < SCREEN_W/2 (left half = previous page) and the right half (next page), spanning full screen height, deliberately larger than the bottom nav strip for under-glass cases. The spec documents the as-built full-half behavior; the README wording is informative and outdated.

2026-06-12 | [MY ERROR] | §3.1 "Nav bar" definition incorrectly called the nav bar the "sole touch target for navigation."
Detail: The as-built touch zones are the full left/right screen halves (handleTouch, sx < SCREEN_W/2), and the nav bar is only a position indicator. Caught during §4.2 review as an internal contradiction; §3.1 corrected to state the nav bar is an indicator and navigation taps span the entire left/right halves.

2026-06-12 | [SESSION] | Drafted WSC spec Rev 1-draft end-to-end (§1–§11, 99 F + 16 NF + 4 IF + 7 UI reqs); descriptive of as-built firmware v1.0.0; Phase 3 consistency audit passed; OI-001/002 resolved, OI-003/004 open.

2026-06-12 | [DECISION] | Resolved OI-003 and OI-004 in code + spec; firmware bumped to v1.1.0.
Detail: OI-004 — display wind, remove bz: wind speed (already fetched in mph) is now shown on the Clock+Weather page on the humidity line (new requirement WSC-F-0100; ui.ino humid buffer enlarged to [24]); the never-populated SpaceWxData.bz field was deleted. OI-003 — TLS: setInsecure() accepted as a v1.1.0 limitation (no code change), retained as documented posture in §5.3/§10. Firmware minor-bumped to v1.1.0 for the wind-display feature; spec target version and affected sections updated accordingly.
