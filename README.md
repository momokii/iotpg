# ESP32 IoT Playground

Learn ESP32 firmware development by building small projects on real hardware —
each one flashed, verified, and documented. CLI-only: no IDE, no Arduino app,
just a terminal, ESP-IDF, and a USB cable.

## Hardware

- **Board:** classic ESP32 DevKit (ESP32-D0WD-V3, 30-pin). Onboard blue LED on
  **GPIO2**, BOOT button on **GPIO0**, serial over CP2102 USB-UART
  (`/dev/ttyUSB0`, `dialout` group required).
- **Modules used:** red-yellow-green LED module (R→GPIO21, Y→GPIO22, G→GPIO23,
  GND→GND, female-to-female jumpers straight to the board).
- **Power:** USB only — unplug to power off; the flashed app auto-starts on
  every plug-in (firmware lives in non-volatile flash).

## Toolchain

ESP-IDF **v6.1**, Xtensa toolchain, esptool, CMake/Ninja — all terminal driven.

```bash
./scripts/setup-esp32.sh            # full install (sudo path, else user-space fallback)
./scripts/setup-esp32.sh --verify   # re-check an existing install
get_idf                             # activate the environment in each new shell
```

New machine? Clone this repo and ask the agent to run the setup script — the
whole environment reproduces from `scripts/setup-esp32.sh`.

## Firmware projects

| # | Project | What it does | Status |
|---|---|---|---|
| 01 | [`firmware/blink/`](firmware/blink/) | Onboard blue LED, 5 s on/off | Done |
| 02 | [`firmware/traffic/`](firmware/traffic/) | R/Y/G sequence, 2 s each | Done |
| 03 | [`firmware/button_lights/`](firmware/button_lights/) | 5 animated shows, BOOT button switches | Done |
| 04 | [`firmware/env_display/`](firmware/env_display/) | DHT11 temp/humidity on auto-discovered LCD/OLED, web dashboard, diagnostics | Done |
| — | [`firmware/pin_sweep/`](firmware/pin_sweep/) | Dev tool: finds unknown lamp GPIOs | Tool |

Only one project runs at a time — flashing replaces what's on the board:

```bash
get_idf
idf.py -C firmware/<name> set-target esp32    # once per fresh checkout
idf.py -C firmware/<name> build
idf.py -C firmware/<name> -p /dev/ttyUSB0 flash monitor
```

Each project README documents its wiring, behavior, and lessons learned.
`firmware/README.md` is the progress log with conventions for adding projects.

## Repo best practices (how this repo stays healthy)

- **One project, one directory** under `firmware/`, with its own README written
  before the task is closed — code without docs doesn't count as done.
- **Conventional Commits**: `feat(scope): …`, `fix(scope): …`, `docs(…): …`,
  `chore(…): …`, with `Refs: TASK-XXX` footers tracing to the task queue.
- **Never commit generated files**: `build/`, `sdkconfig`, `sdkconfig.old`,
  `managed_components/` are gitignored. Commit `sdkconfig.defaults*`,
  `dependencies.lock`, and sources.
- **Prove it before committing**: every firmware commit was built clean, and
  every behavior commit was verified on the board via monitor (or
  `verify-flash` digest match). Monitor logs are the test suite for hardware.
- **Small commits, one idea each**: setup, feature, fix, and docs stay separate.
- **Secrets never committed**: `.env` files are gitignored; `.env.example`
  holds placeholder keys only.
- **USB out while rewiring.** Always.
- **Agent system**: `.claude/` holds the working method — orientation
  (`README.md`), behavioral rules (`AGENT_RULES.md`), standards, task queue
  and decision log under `state/`, checklists under `templates/`. Any agent
  opening this repo cold starts at `.claude/HOW_TO_RESUME.md`.
