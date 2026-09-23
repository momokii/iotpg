## Project Phase

Toolchain ready — ESP32 firmware playground bootstrapped. No hardware yet.

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

## In Progress

- [ ] Awaiting ESP32 board arrival / user direction for first firmware project

## Blocked

- Flash/monitor verification blocked on hardware: no `/dev/ttyUSB*` device
  present. Build-only verification done; `idf.py flash` runs when board arrives.

## Open Questions

- Which ESP32 variant will arrive (classic ESP32, S3, C3)? Affects `set-target`.
- First firmware project scope (blink/GPIO → Wi-Fi → sensors?)

## Security Notes

- No firmware written yet. Wi-Fi secrets policy set: menuconfig/Kconfig for dev
  only, NVS/provisioned partition for real secrets — never hardcoded.

## Last Updated

2026-09-24 — Session: ESP32 toolchain installed + verified, repo made
self-provisioning (`scripts/setup-esp32.sh`).
Next: plug in ESP32 board → run `idf.py -p <PORT> flash monitor` on hello_world
→ pick first firmware project in TASK_QUEUE.md.
