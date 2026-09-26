# Project 04 — Env display (DHT11 temp/humidity on LCD/OLED)

Reads a DHT11 sensor every 2 s, logs to USB, and mirrors the readings to
whichever display is attached: SSD1306 OLED and/or 1602 LCD with PCF8574
backpack. The firmware auto-discovers displays — see below.

- **Board:** classic ESP32 (ESP32-D0WD-V3), `set-target esp32`.
- **Status:** sensor + 1602 LCD verified (see log below); SSD1306 path written
  but not yet seen live. Closed as TASK-007 after visual confirmation.

## Screens

One unified screen, refreshed every second (sensor re-read every 2 s):

- SSD1306: `HH:MM:SS`, `DD-MM-YY SRC`, `T:25.0C H:60%` on separate rows.
- 1602 LCD: `T:25.0C H:60%` on line 0, `HH:MM:SS DD-MM` on line 1.
- SRC is `RTC` (DS3231), `NTP` (Wi-Fi sync), or `INT` (internal free-run);
  shown in full on SSD1306 and USB log (1602 has no room for it).

(Humidity prints without decimals on purpose: the DHT11 only resolves
whole percents. Temperature keeps one decimal so a future DHT22 swap
shows tenths with no format change.)

## Time sources (offline precision)

No network needed, but precision depends on hardware you attach:

| Source | Precision | Needs |
|---|---|---|
| DS3231 RTC module (auto-detected at 0x68) | ±2 ppm ≈ seconds per year, battery keeps time across power loss | DS3231 module + coin cell; wire SDA/SCL anywhere, firmware finds it |
| NTP over Wi-Fi (active when configured) | exact, re-synced hourly | SSID/password via menuconfig (below); source tag `NTP` |
| Internal clock (fallback) | starts at firmware build moment, drifts minutes per day, resets on power loss | nothing |

Priority is DS3231 > NTP > internal. Timezone is WIB (UTC+7).

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

## Build / flash / monitor

```bash
get_idf
idf.py -C firmware/env_display set-target esp32   # once per fresh checkout
idf.py -C firmware/env_display build
idf.py -C firmware/env_display -p /dev/ttyUSB0 flash monitor
```

Expected monitor output: `Display ACK at 0x.. on SDA=.. SCL=..`,
`<driver> ready`, then `Temp: 25.0 C Hum: 42.0 %` every 2 s.
