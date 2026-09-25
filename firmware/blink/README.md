# Project 01 — Blink (onboard blue LED)

First firmware ever flashed on this board. Blinks the ESP32 DevKit's onboard
blue LED (GPIO2): 5 seconds on, 5 seconds off, forever.

- **Origin:** adapted from ESP-IDF v6.1 `examples/get-started/blink`
  (plain-GPIO mode, `BLINK_LED_GPIO`).
- **Board:** classic ESP32 (ESP32-D0WD-V3), `set-target esp32`.
- **Status:** DONE — flashed, hash-verified, confirmed on monitor timestamps
  and seen physically. Closed as TASK-002.

## Wiring

None. Uses only the onboard LED. USB power is enough.

## Configuration

`sdkconfig.defaults.esp32`:

```
CONFIG_BLINK_GPIO=2
CONFIG_BLINK_PERIOD=5000
```

## Build / flash / monitor

```bash
get_idf
idf.py -C firmware/blink set-target esp32   # once per fresh checkout
idf.py -C firmware/blink build
idf.py -C firmware/blink -p /dev/ttyUSB0 flash monitor
```

Expected monitor output: `Turning the LED ON/OFF!` alternating every ~5000 ms,
and the blue LED visibly blinking.

## What was learned

- ESP-IDF workflow end to end: `set-target → build → flash → monitor`.
- `verify-flash` digest matching proves exactly which binary is on the board.
- GPIO2 is a strapping pin but safe to *drive* at runtime (strapping is only
  sampled at reset while pins are high-Z).
