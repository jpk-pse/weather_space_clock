# Open Issues — WSC (Weather + Space Clock)

Persistent OI log. Supplements §11 of the spec (which is frozen between revisions) and provides continuity across sessions. Update whenever an OI is opened or resolved.

| OI ID | Description | Owner | Status | Resolution |
|---|---|---|---|---|
| OI-001 | First-frame behavior when the one-time initial fetch (WSC-F-0010) fails before any last-good value exists. | James | Resolved | Verified at `ui.ino` `drawClockPage`: with `w.ok==false` the page renders "(loading weather...)" and still draws the clock/date; day/night theme falls back to the local hour. Captured by WSC-F-0040/0041. |
| OI-002 | "ISS visibility state" displayed by WSC-F-0049 and WSC-F-0050 is not defined in §3.1 and has no enumeration in §3.3. | James | Resolved | Verified at `space.ino` `fetchISS`: visibility is a verbatim passthrough of the WhereTheISS.at (N-2) `visibility` field, values `daylight`/`eclipsed`, with `?` substituted when absent. Added as a §3.1 definition; no on-device transformation. |
| OI-003 | HTTPS uses `setInsecure()` (no TLS certificate validation, WSC-F-0091 / WSC-NF-0010). Accepted limitation: device has no cert store and exchanges only read-only public data. | James | Resolved | Accepted for v1.1.0 (no code change); documented posture in §5.3 and §10 (C-4). |
| OI-004 | Two unused struct fields: `WeatherData.windMph` populated (mph) but displayed nowhere; `SpaceWxData.bz` (nT) declared but never populated. | James | Resolved | v1.1.0: wind now displayed on Clock+Weather page (WSC-F-0100, ui.ino); `bz` field removed from SpaceWxData (weather_space_clock.ino). |
