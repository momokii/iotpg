# Project 03 — Button light shows (BOOT switches animation)

Five animated light shows on the R/Y/G module. Each press of the onboard
**BOOT** button interrupts the running show and jumps to a random different
one (never repeats the same show twice).

| # | Show | Animation |
|---|---|---|
| 0 | CHASE | Single lamp running R→Y→G, 400 ms each |
| 1 | TRAFFIC | Classic R 2 s → Y 1 s → G 2 s |
| 2 | BLINK | All three together, 500 ms on/off |
| 3 | BUILD-UP | R, then R+Y, then all, hold, off |
| 4 | PING-PONG | R→Y→G→Y→R bounce, 350 ms each |

- **Board:** classic ESP32 (ESP32-D0WD-V3), `set-target esp32`.
- **Status:** DONE — flashed, boot verified on monitor, user confirmed
  presses switch shows. Closed as TASK-005.

## Wiring

Same module wiring as Project 02 (R→GPIO21, Y→GPIO22, G→GPIO23, GND→GND)
plus **nothing** — the button is the onboard BOOT button (GPIO0 → GND when
pressed), internal pull-up, 50 ms debounce.

> History: an external button was tried first on GPIO19 then GPIO18, but
> presses never registered electrically (pin read HIGH through holds), so the
> design moved to the onboard BOOT button. The external-button attempts are
> recorded in git history (`764e035`, `cd98dd3`).
>
> GPIO0 strapping caveat: hold BOOT only while the app runs, never across
> reset/power-up (that enters download mode). If the board ever seems stuck,
> tap EN without touching BOOT.

## Build / flash / monitor

```bash
get_idf
idf.py -C firmware/button_lights set-target esp32   # once per fresh checkout
idf.py -C firmware/button_lights build
idf.py -C firmware/button_lights -p /dev/ttyUSB0 flash monitor
```

Expected monitor output: `Show N: <NAME> — press BOOT to change` on boot and
after every press. Button level is readable remotely (temporary `DBG button`
logging was used during bring-up; see git history).

## What was learned

- Interruptible sleeps (poll button every 10 ms inside animation timing) keep
  input responsive without an RTOS task per show.
- Random-without-repeat: `esp_random() % N` loop excluding current index.
- Onboard BOOT is the standard zero-wiring button for demos and mode switching.
