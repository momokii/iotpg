## Task Queue

> Stack confirmed: ESP-IDF v6.1, CLI-only. Backlog below — agent works top-down.

| Task ID | Name | Priority | Status | Complexity | Depends On | Scope | Acceptance Criteria | Security Concerns |
|---|---|---|---|---|---|---|---|---|
| TASK-001 | Verify flash on real ESP32 board | High | DONE | S | — | Plug board, find port, `idf.py -p <PORT> flash monitor` hello_world | "Hello world!" + chip info in monitor output | Confirm user in `dialout`; never `erase-flash` a non-dev board without asking |
| TASK-002 | First firmware: GPIO blink in `firmware/blink/` | High | DONE (GPIO2 onboard blue LED, 5 s on/off, user confirmed visible) | S | TASK-001 | IDF blink example adapted to onboard LED pin of user's board | LED blinks, build clean, runs after power cycle | None (no network, no secrets) |
| TASK-003 | Wi-Fi station + MQTT telemetry sketch | Medium | TODO | M | TASK-002 | Connect to AP, publish sensor/hello message to broker | Messages arrive at broker; reconnects on AP drop | Secrets via Kconfig dev-only / NVS for real; validate broker TLS before sending anything sensitive |
| TASK-004 | Traffic-light module (R/Y/G) on GPIO 21/22/23 | High | DONE (direct female-to-female wiring; breadboard was the fault; 2 s cycle confirmed on monitor) | S | TASK-002 | `firmware/traffic`: red→yellow→green 2 s each on module wired R→21 Y→22 G→23 | Lamps cycle visibly; order matches R/Y/G assumption (user to confirm sequence) | None (no network, no secrets) |
| TASK-005 | Button-controlled light shows via onboard BOOT button | High | DONE (5 animated shows, BOOT press switches show, user confirmed working) | M | TASK-004 | `firmware/button_lights`: CHASE/TRAFFIC/BLINK/BUILD-UP/PING-PONG, random on press | Press changes show visibly; no repeat of same show | None (no network, no secrets; GPIO0 strapping caveat documented) |
| TASK-006 | Project docs + progress log (per-project READMEs, firmware index) | Medium | DONE (4 docs written, all 3 projects rebuilt clean) | S | TASK-005 | README per project + `firmware/README.md` tracker | Every project documented and rebuildable | None |
| TASK-007 | Env station: DHT11 + auto-discovered display | High | DONE (DHT live; 1602 @ 0x27 text confirmed on glass by user; contrast screw was the final fix) | M | TASK-005 | `firmware/env_display`: sensor + SSD1306/1602 drivers, I2C auto-discovery | Readings on USB + text on attached display(s) | None (no network/secrets; display sharing one pin with DHT is fragile — prefer one signal per pin) |
| TASK-008 | Clock screen on BOOT toggle (DS3231/internal time) | High | DONE (BOOT toggles env/clock both ways, user confirmed on glass) | S | TASK-007 | Clock view + time sources in `firmware/env_display` | Clock shows HH:MM:SS + date + source tag | None |
| TASK-009 | NTP time sync over static Wi-Fi (WIB timezone) | High | DONE (joined AP in ~2 s, NTP acquired; source tag `NTP`; creds in gitignored sdkconfig only) | S | TASK-008 | `firmware/env_display` WiFi station + SNTP, hourly re-sync | Monitor shows join + `NTP sync acquired`; clock screen shows NTP time | None (SSID/password never in code or git — verified by grep before commit) |
| TASK-010 | Dynamic Wi-Fi provisioning (no reflash to change networks) | Medium | TODO | M | TASK-009 | Captive-portal or serial-console credential setup, NVS storage | Change networks without rebuild | Secrets must stay out of git; validate input at boundary |
| TASK-011 | Unified single screen (time+date+env, no toggle) | Medium | DONE (one view refreshed every second; verified on monitor + 1602 init) | S | TASK-009 | Merge env + clock views in `firmware/env_display` | All info visible at once, no button needed | None |
| TASK-012 | Jakarta timestamp, drop seconds (HH:MM + full date) | Low | DONE (WIB already == Asia/Jakarta; verified on monitor) | S | TASK-011 | Simplify clock format in `firmware/env_display` | Time reads HH:MM, date has year on 1602 | None |
| TASK-013 | Show timezone on screen (WIB tag) | Low | DONE (SSD1306 date line + 1602 line 1 carry WIB; verified on monitor) | S | TASK-012 | Timezone indicator in `firmware/env_display` | Screen states which zone the time is in | None |
| TASK-014 | BOOT toggles WIB/UTC timezone with proper dates | Low | DONE (toggle verified in code + boot log; user to confirm glass) | S | TASK-013 | Timezone mode in `firmware/env_display` | One click flips zone + date correctly | None (GPIO0 strapping caveat documented) |
| TASK-015 | WiFi info page + short/long-press button UX, provisioning-ready seam | Medium | DONE (boot verified; user to confirm hold-flips-page on glass) | M | TASK-014 | Pages + `wifi_status_t` in `firmware/env_display` | SSID/status/RSSI visible; hold flips page, short toggles zone | None |
| TASK-016 | WiFi geolocation + place names + persistent last fix | High | DONE (live fix Bogor Jawa Barat ±25 km; NVS cache; stale handling; verified on monitor) | M | TASK-015 | Geo pipeline + Where page in `firmware/env_display` | Coords + place + accuracy + age on screen; survives offline/reboot | Nearby AP MACs sent to BeaconDB (learning-OK, eyes open) |
| TASK-017 | Scrolling place marquee + accuracy/age replacing raw coords | Low | DONE (bouncing marquee per screen width; verified boot on monitor) | S | TASK-016 | Marquee + human Where page in `firmware/env_display` | Long names scroll instead of truncating; no lat/lon on glass | None |
| TASK-020 | Silence task-watchdog warnings during geo TLS work | Low | DONE (WDT timeout 5 s → 30 s with rationale comment; clean 3-min soak, zero warnings/resets) | S | TASK-016 | Watchdog tuning in `firmware/env_display/sdkconfig.defaults` | Long geo lookup completes without warnings | None |
| TASK-021 | Humanize Where page (plain words, scrolling names, new glyphs) | Low | DONE (Here/Near/Around/Rough + just now/5m ago; verified boot on monitor) | S | TASK-016 | Human copy + font coverage in `firmware/env_display` | Non-technical readers understand every line | None |
| TASK-022 | Self-explanatory Stats page (plain words, alternating views) | Low | DONE (Today/Feels/Dew point wording; verified boot on monitor) | S | TASK-019 | Human copy in `firmware/env_display` | A stranger reads the screen with no manual | None |
| TASK-023 | Stats states its place link (third view with @place + freshness) | Low | DONE (verified boot on monitor; user to confirm glass) | S | TASK-022 | Place-linked stats in `firmware/env_display` | Stats never silently borrow a stale location | None |
| TASK-024 | Where info alternates human words with within-Xkm explainer | Low | DONE (verified boot on monitor) | S | TASK-016 | Explainer line in `firmware/env_display` | km figure never appears without its meaning | None |
| TASK-026 | Web dashboard on local WiFi (readings page, no creds) | High | DONE except LAN-reachability: server starts on join, all 15 markers map 1:1, template 1729 B; AP client isolation blocks LAN clients (evidence: gateway reachable, board ARP absent) — home-AP use unaffected | M | TASK-009 | Dashboard service in `firmware/env_display` | GET / 200 with live values where reachable | None (page carries readings only; audit clean) |
| TASK-027 | Diagnostics snapshot + display page + reboot persistence | Medium | DONE (boot #6→#8 across resets proves NVS persist; page 5 wired; no aborts) | S | TASK-026 | Diagnostics in `firmware/env_display` | Sane uptime/heap/boots/RSSI; count survives reboot | None |
| TASK-028 | Custom 1.5 MiB partition table (dashboard no longer fits 1 MiB) | Medium | DONE (factory 0x10000 1536K verified; NVS offsets preserved; clean boot after) | S | TASK-026 | partitions.csv in `firmware/env_display` | Image fits with 32% headroom | None |
| TASK-025 | Outdoor weather page (Open-Meteo, feels-like, rain chance) | High | DONE (live: 25.4C 88% feels 30.2C Fair rain 46%; NVS cache; verified on monitor) | M | TASK-016 | Weather pipeline + Outside page in `firmware/env_display` | Indoor vs outdoor side by side, no key, no hardware | Nearby AP MACs already disclosed for geo; forecast adds nothing new |
| TASK-019 | Stats page: min/max + trend + comfort + dew point | Medium | DONE (NVS restore verified on boot; user to confirm glass on page 3) | M | TASK-007 | Stats module + page in `firmware/env_display` | Extremes survive reboot; comfort grounded in ASHRAE bands | None |
| TASK-018 | Re-sync clock immediately on WiFi reconnect | Low | DONE (restart SNTP on first IP after a drop; boot verified, drop test left to user) | S | TASK-009 | Reconnect handling in `firmware/env_display` | Clock self-heals seconds after Wi-Fi returns | None |

### Template Format (use for every task added)

| Field               | Value                                              |
|---------------------|----------------------------------------------------|
| Task ID             | TASK-001                                           |
| Name                | [Task name]                                        |
| Priority            | High / Medium / Low                                |
| Status              | TODO / IN PROGRESS / DONE / BLOCKED                |
| Complexity          | S / M / L                                          |
| Depends On          | [Task IDs this task requires to be done first]     |
| Scope               | [Exact description of what must be built]          |
| Acceptance Criteria | [What "done" looks like, measurable]               |
| Security Concerns   | [Any security considerations specific to this task]|
