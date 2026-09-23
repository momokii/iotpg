# Environment Guide — Definitions & Agent Behavior per Environment

## This Project's Environments (ESP-IDF v6.1, verified 2026-09-24)

There is no cloud staging/production here — the "environments" are the
**host build machine** and the **ESP32 target board**:

| Environment | Purpose | Characteristics |
|---|---|---|
| `host` (dev machine) | Build firmware, flash, monitor logs | ESP-IDF v6.1 at `~/esp/esp-idf-v6.1`, tools in `~/.espressif`, verbose monitor output encouraged |
| `target` (ESP32 board) | Run firmware | No debug backdoor beyond IDF Monitor over USB serial; flashed binaries are release unless `menuconfig` says otherwise |

**Machine setup (reproducible):** `./scripts/setup-esp32.sh` — installs system
prerequisites (via `apt` with sudo, or user-space `~/.local/esp-sysroot` without
sudo), clones ESP-IDF v6.1, runs `install.sh esp32`, installs the `get_idf`
shell alias, and verifies by building `hello_world`. Run
`./scripts/setup-esp32.sh --verify` to re-check an existing install.

**Verified toolchain locations (this machine):**

| Tool | Location / version |
|---|---|
| ESP-IDF | `~/esp/esp-idf-v6.1`, tag `v6.1` |
| Python env | `~/.espressif/python_env/idf6.1_py3.12_env` |
| Toolchains | `~/.espressif/tools` |
| esptool | v5.4.0 (bundled with IDF) |
| cmake/ninja/flex/bison/gperf | `~/.local/esp-sysroot/usr/bin` (user-space, no sudo) |

## Real Commands (verified working)

```bash
get_idf                                        # activate IDF env (alias; else: . ~/esp/esp-idf-v6.1/export.sh)
idf.py --version                               # sanity check → ESP-IDF v6.1
idf.py set-target esp32                        # per project, once (wipes build/ + sdkconfig)
idf.py build                                   # produces build/<name>.bin
idf.py -p /dev/ttyUSB0 flash                   # flash (default baud 460800)
idf.py -p /dev/ttyUSB0 monitor                 # serial monitor, quit with Ctrl+]
idf.py -p /dev/ttyUSB0 flash monitor           # flash + monitor in one go
idf.py -p /dev/ttyUSB0 erase-flash             # full wipe — confirm with user first
python -m esptool -p /dev/ttyUSB0 chip-id      # board detect / connectivity check
```

Find the port: run `ls /dev/ttyUSB* /dev/ttyACM*` before and after plugging the
board. Native-USB chips (S2/S3/C3) show as `/dev/ttyACM0`. User must be in the
`dialout` group (`sudo usermod -a -G dialout $USER`, then re-login) or flashing
fails with "Permission denied".

## General Environment Definitions (for future networked services)

| Environment | Purpose | Characteristics |
|---|---|---|
| `development` | Local development and feature work | Debug mode on, verbose logging, hot reload, relaxed auth optional, no real external services required |
| `staging` | Pre-production validation | Mirrors production config, uses real (sandboxed) services, no debug mode |
| `production` | Live system | No debug, minimal logging, hardened config, real services and secrets |

## Agent Behavior by Environment

In `development`:

- Verbose logging is acceptable and encouraged for debugging.
- Debug ports and tools may be exposed (e.g., database GUIs, profilers).
- Seed data scripts and fixtures may be run freely.
- Hot reload and volume mounts are expected in Docker Compose.

In `staging` or `production`:

- The agent must never run destructive commands (`DROP`, `DELETE`, `TRUNCATE`,
  irreversible migrations, `erase-flash` on a non-dev board) without explicit written confirmation from the user.
- The agent must never directly modify production config files or secrets.
- Any proposed change must be presented as a written plan first — not executed
  immediately.
- The agent must flag explicitly if it detects it is operating in a
  non-development context.

## Docker Compose Environment Pattern

All Docker-based projects must follow this override pattern:

- `docker-compose.yml` — base service definitions, environment-agnostic.
- `docker-compose.override.yml` — development overrides: hot reload, debug ports,
  volume mounts for live code; loaded automatically by Docker Compose.
- `docker-compose.prod.yml` — production overrides: no volume mounts, resource
  limits, restart policies, no exposed debug ports; loaded explicitly with `-f`.

```bash
# Development (automatic — docker-compose.override.yml is loaded by default)
docker-compose up

# Production (explicit — only base + prod override)
docker-compose -f docker-compose.yml -f docker-compose.prod.yml up -d
```

The agent must always know which command applies to the current context and must
**always ask** before running any Compose command that is not clearly development.

## `.env` File Pattern

```
.env.example        # Committed to repo — all keys with placeholder values + comments
.env                # Never committed — actual development secrets
.env.staging        # Never committed — staging secrets
.env.production     # Never committed — production secrets
```

## First-Session Environment Checklist

Before the first commit of any session, the agent must verify:

- [ ] `.env` (and `.env.staging`, `.env.production`) are listed in `.gitignore`
- [ ] `.env.example` exists at the root with all required keys and placeholder values
- [ ] `APP_ENV` (or equivalent) is set and the active environment is identified
- [ ] No secret value appears in any staged file (`git status` + `git diff --cached`)
- [ ] (Firmware) `get_idf` activates cleanly and `idf.py --version` reports v6.1
- [ ] (Firmware) `firmware/*/build/`, `sdkconfig`, `sdkconfig.old` are gitignored
