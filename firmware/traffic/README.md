# Project 02 — Traffic light (R/Y/G, static sequence)

Drives a red-yellow-green LED module in a fixed repeating sequence:
RED 2 s → YELLOW 2 s → GREEN 2 s. The stepping stone between the single
onboard blink and the button-controlled shows.

- **Board:** classic ESP32 (ESP32-D0WD-V3), `set-target esp32`.
- **Status:** DONE — flashed, cycle verified on monitor at exact 2000 ms
  intervals. Closed as TASK-004.

## Wiring (USB OUT while wiring)

Female-to-female jumpers, module straight to board (no breadboard — an early
breadboard attempt stayed dark; direct wiring worked immediately):

| Module pin | ESP32 pin |
|---|---|
| R | GPIO21 |
| Y | GPIO22 |
| G | GPIO23 |
| GND | GND |

Module pin order assumed as R/Y/G (to be confirmed visually; if the visible
order differs, remap `LED_RED/YELLOW/GREEN` in `main/main.c` — one-line change
per pin). All three GPIOs are non-strapping, non-UART, verified by `pin_sweep`.

## Build / flash / monitor

```bash
get_idf
idf.py -C firmware/traffic set-target esp32   # once per fresh checkout
idf.py -C firmware/traffic build
idf.py -C firmware/traffic -p /dev/ttyUSB0 flash monitor
```

Expected monitor output: `RED ON → YELLOW ON → GREEN ON` every 2000 ms,
lamps following the same order.

## What was learned

- A dark lamp with proven firmware means a circuit fault, not a code fault —
  diagnose wiring first (`verify-flash` + monitor prove the board side).
- Breadboard rails/gaps are a whole failure category; direct jumpers remove it.
- `esptool verify-flash` gives cryptographic proof of what's on the chip.
