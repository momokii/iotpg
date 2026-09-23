## Project Phase

First hardware live — ESP32 board connected and running blink firmware.

## Completed

- [x] .claude/ agent infrastructure initialized
- [x] ESP-IDF v6.1 + Xtensa toolchain installed (this machine, no-sudo user-space
  path: `~/esp/esp-idf-v6.1`, tools in `~/.espressif`, build prereqs in
  `~/.local/esp-sysroot`)
- [x] End-to-end build verified: `hello_world` → `hello_world.bin` (123 KB) for
  `esp32` target, esptool v5.4.0
- [x] `scripts/setup-esp32.sh` created — reproduces full setup on any new machine
- [x] `.claude/` updated with ESP-IDF project context (README, CODING_STANDARDS,
  ENVIRONMENT_GUIDE) + `.gitignore` covers firmware build artifacts
- [x] TASK-001: board detected as `/dev/ttyUSB0` (CP2102), chip ESP32-D0WD-V3
  rev 3.1, user already in `dialout`
- [x] TASK-002: `firmware/blink` (GPIO5, 1 s period) built, flashed,
  hash-verified; monitor confirmed app running ("Turning the LED" ~1/sec).
  Note: an early transient `rst:` reboot loop cleared on its own — suspected
  flaky breadboard contact at monitor attach, not firmware.

## In Progress

- [ ] TASK-002 physical confirmation: user reported only the red power LED is
  on — breadboard lamp is dark, so it is NOT on GPIO5. `firmware/pin_sweep`
  is flashed and running (16 safe GPIOs, 4 s HIGH each, repeating; verified
  cycling on monitor). Awaiting user report of when the lamp lights (or the
  pin label read off the board silkscreen). Then lock blink to that pin.

## Blocked

## Blocked

- TASK-002 physical confirmation needs the user's eyes: is the breadboard lamp
  itself blinking, and which pin is it wired to? Firmware side is proven working.

## Open Questions

- Breadboard lamp pin (user unsure) — currently firmware blinks GPIO5.
- Board variant confirmed: classic ESP32 (ESP32-D0WD-V3, `set-target esp32`).

## Security Notes

- No network firmware yet. Wi-Fi secrets policy set: menuconfig/Kconfig for dev
  only, NVS/provisioned partition for real secrets — never hardcoded.

## Last Updated

2026-09-24 — Session: lamp dark on GPIO5 blink → pin_sweep flashed, verified
cycling (GPIO 5→4→13→14… on monitor). Committed as 5afa0ec.
Next: user reports when lamp lights → lock blink to that pin, re-flash.
