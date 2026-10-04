# Project 04 — Env display (DHT11 temp/humidity on LCD/OLED)

Reads a DHT11 sensor every 2 s, logs to USB, and mirrors the readings to
whichever display is attached: SSD1306 OLED and/or 1602 LCD with PCF8574
backpack. The firmware auto-discovers displays — see below.

- **Board:** classic ESP32 (ESP32-D0WD-V3), `set-target esp32`.
- **Status:** sensor + 1602 LCD verified (see log below); SSD1306 path written
  but not yet seen live. Closed as TASK-007 after visual confirmation.

## Screens

One unified screen, refreshed every second (sensor re-read every 2 s).
Clock shows hours:minutes only (Jakarta/WIB time, no seconds), plus date:

- SSD1306: big `HH:MM`, small `DD-MM WIB SRC`, small `T:25.0C H:60%`.
- 1602 LCD: `T:25.0C H:60%` on line 0, `HH:MMWIB DD-MM` on line 1
  (16 columns exactly — no room for spaces around WIB).
- SRC is `RTC` (DS3231), `NTP` (Wi-Fi sync), or `INT` (internal free-run);
  shown in full on SSD1306 and USB log (1602 shows the WIB zone instead —
  WIB == UTC+7 == Asia/Jakarta, no daylight saving, so the zone alone
  disambiguates).

## Pages and button UX (BOOT, GPIO0)

- Page 0 — main: time/date/env as above.
- Page 1 — wifi, in plain words: network name, then `Signal: Strong /
  Good / OK / Weak`, or `Not connected`. No decibels on glass (dBm stays
  in the USB log for diagnostics).
- Page 2 — where: place name (scrolls if long), accuracy + fix age.
- Page 3 — stats: today's min/max temp+humidity, trend arrow (^ up,
  v down, - flat vs 10 min ago), comfort word, dew point.
- Page 2 — where: place name, coordinates, accuracy + age. Examples:
  `Bogor, Jawa Barat` / `-6.59444,106.78900` / `+-25000m fresh`. Stale
  fixes show `OLD` plus minutes since update; with no fix ever it says
  `no fix yet / wait for lookup`. NVS keeps the last fix across reboots.
- Short press: toggles WIB/UTC timezone. Long hold (~1.5 s): flips pages.
  Fire-on-threshold for hold, fire-on-release for short — the standard
  single-button pattern; more pages slot into the `page` switch.
  (GPIO0 strapping caveat: press only while the app runs, never across
  reset — tap EN alone to recover.)
- Display code consumes Wi-Fi state only through `wifi_status_t`
  (`wifi_get_status()`) — never Kconfig or esp_wifi calls. Dynamic
  provisioning later fills the same struct; consumers don't change.

(Humidity prints without decimals on purpose: the DHT11 only resolves
whole percents. Temperature keeps one decimal so a future DHT22 swap
shows tenths with no format change.)

## Text sizes (SSD1306 only — 1602 cells are fixed hardware)

- Big clock line: vendored 12x6 font (`oled_font.*`, Apache-2.0).
- Date/env lines: compact hand-built 5x7 subset (`main/font57.h`, digits +
  the letters/punctuation these screens print) — 21 chars/line, leaving
  the bottom half of the 128x64 glass free for future rows.

## Time sources (offline precision)

No network needed, but precision depends on hardware you attach:

| Source | Precision | Needs |
|---|---|---|
| DS3231 RTC module (auto-detected at 0x68) | ±2 ppm ≈ seconds per year, battery keeps time across power loss | DS3231 module + coin cell; wire SDA/SCL anywhere, firmware finds it |
| NTP over Wi-Fi (active when configured) | exact, re-synced hourly | SSID/password via menuconfig (below); source tag `NTP` |
| Internal clock (fallback) | starts at firmware build moment, drifts minutes per day, resets on power loss | nothing |

Priority is DS3231 > NTP > internal. Timezone is WIB (UTC+7).

Reconnects re-sync immediately: any drop auto-retries the join, and the
first new IP restarts SNTP on the spot (plus the regular hourly
re-sync), so the clock self-heals seconds after Wi-Fi returns instead
of waiting out the hour.

## Wi-Fi config (static for now)

Credentials live in Kconfig with **empty** committed defaults
(`sdkconfig.defaults`). Set real values per machine:

```bash
idf.py -C firmware/env_display menuconfig   # Component config → WiFi station
```

They land in the gitignored `sdkconfig` — never committed, never in code.
Empty SSID disables WiFi silently (time falls back gracefully). Changing
networks later = edit + rebuild + reflash. Dynamic provisioning (captive
portal, no reflash) is a planned future task, not yet implemented.

## Wiring actually found (not as labeled)

The expansion shield's labels proved untrustworthy, so the firmware probes
every SDA/SCL pair among GPIO 4/5/12/13/14/21/22/23 for display addresses
(SSD1306 0x3C/0x3D, PCF8574 0x27/0x3F) and uses whatever answers:

| Signal | Discovered live | Notes |
|---|---|---|
| DHT `out` | GPIO21 | `+`→3V3, `−`→GND (power header) |
| 1602 SDA/SCL | SDA=23, SCL=22 | PCF8574 @ 0x27 (note: swapped vs user's belief) |
| SSD1306 | not present this boot | driver ready when 0x3C answers |

DHT note: defaults to DHT11 (`DHT_TYPE_DHT11`); for DHT22 change to
`DHT_TYPE_AM2301` — wiring identical. DHT `out` must be alone on its pin:
sharing GPIO21 with OLED SCL caused intermittent NACKs on both.

## Key engineering decisions

- **espressif/ssd1306 rejected:** targets legacy `driver/i2c.h`, removed in
  ESP-IDF v6. This project drives SSD1306 directly on `driver/i2c_master.h`
  (`oled.c`) and vendors only that component's Apache-2.0 font table.
- **`esp-idf-lib/dht`** from the component registry for the sensor.
- **1602 via PCF8574** (`lcd1602.c`): standard backpack wiring
  (P0=RS, P1=RW, P2=EN, P3=backlight, P4–P7=D4–D7), 4-bit HD44780 init.
- **Graceful degradation:** missing display or sensor never aborts boot —
  warnings on USB, sensor keeps logging.
- **Crash-loop lesson:** moving wires while powered browned out the board and
  corrupted flash twice; recovery is reflash of any known-good build.
  USB out before touching wires, always.
- **Heavy network work gets its own task:** the geo lookup (blocking scan,
  TLS handshake, JSON parse) overflowed app_main's 3.5 KB stack and rebooted
  the board every ~16 s. It now runs in a dedicated 12 KB task; displays
  must additionally never abort (all I2C failures disable gracefully).

## Stats page (daily extremes, trend, comfort, dew point)

- Min/max temp+humidity with timestamps, persisted in NVS and reset on
  date rollover — reboots never lose today's extremes.
- Trend arrow vs 10 minutes ago (`^`/`v`/`-`, ±0.5 °C deadband).
- Comfort word from ASHRAE-grounded bands: 30–60 % RH comfortable;
  ≥70 % muggy (mold watch), <30 % dry; over 30 °C hot, under 18 °C cold.
- Dew point via Magnus-Tetens (b=17.625, c=243.04): the number that
  predicts condensation and mold risk better than RH alone.

Field guide (what each piece means on glass):
`Hi26C` = today's highest temp · `Lo24C` = today's lowest ·
`^` rising / `v` falling / `-` flat vs 10 min ago ·
`Comfort` = verdict for the room right now ·
`Dew18C` = dew point — surfaces colder than this get wet.

## Location (no GPS hardware)
WiFi positioning, verified live: scan nearby APs → BeaconDB geolocate
(keyless) → Nominatim reverse-geocode (keyless) → city + province on
screen. First real fix: `-6.59444,106.78900`, `Bogor, Jawa Barat`,
±25 km — street-level needs denser BeaconDB coverage here; the API's
accuracy radius always says how much to trust. Lookups run 15 s after
boot then every 15 min; the last fix persists in NVS across reboots.

## Build / flash / monitor

```bash
get_idf
idf.py -C firmware/env_display set-target esp32   # once per fresh checkout
idf.py -C firmware/env_display build
idf.py -C firmware/env_display -p /dev/ttyUSB0 flash monitor
```

Expected monitor output: `Display ACK at 0x.. on SDA=.. SCL=..`,
`<driver> ready`, then `Temp: 25.0 C Hum: 42.0 %` every 2 s.
