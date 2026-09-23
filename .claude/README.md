# Project Orientation — Read This First

> All `.claude/` files start intentionally general and stack-agnostic.
> They are expected to evolve: after every working session the agent must
> replace placeholder content with accurate, project-specific knowledge.
> General-first is a starting point, not a permanent state.

## What This Repository Is

ESP32 IoT playground — iterative AI-assisted firmware development ("vibe coding"
for microcontrollers), starting with the classic ESP32 chip.

- **Stack (confirmed 2026-09-24):** ESP-IDF **v6.1** (Espressif's official SDK),
  Xtensa toolchain, esptool v5.4.0, CMake/Ninja build. CLI-only workflow —
  no IDE, no Arduino IDE, no VS Code required.
- **Reproducible setup:** `./scripts/setup-esp32.sh` provisions the full toolchain
  on any Debian/Ubuntu machine (sudo path if available, user-space fallback
  without sudo). On a new machine: clone repo → ask agent to run the setup script.
- **Firmware lives in:** `firmware/` (one IDF project per directory).
- **Daily workflow:** `get_idf` (activate env) → `idf.py set-target esp32` →
  `idf.py build` → `idf.py -p /dev/ttyUSB0 flash monitor`.

**Current phase:** Toolchain ready — ESP-IDF v6.1 installed, `hello_world`
build verified for `esp32`. No hardware connected yet; flash step deferred until
the ESP32 board arrives (see `state/CURRENT_STATUS.md` and `state/TASK_QUEUE.md`).

## Agent Orientation Sequence

Read these files in order at the start of **every** session
(full protocol in `HOW_TO_RESUME.md`):

1. `HOW_TO_RESUME.md` — the numbered resume protocol. Start here, every time.
2. `state/CURRENT_STATUS.md` — exact current state: done, in progress, blocked.
3. `state/TASK_QUEUE.md` — the ordered backlog; identifies the next task.
4. `AGENT_RULES.md` — non-negotiable behavioral rules for every session.
5. `CODING_STANDARDS.md` — conventions to follow before writing any code.
6. `SECURITY_STANDARDS.md` — security requirements; mandatory before any
   implementation involving input, auth, external services, or storage.
7. `ENVIRONMENT_GUIDE.md` — environment definitions and per-environment behavior.
8. Task-relevant docs (PRD section, architecture doc, API contract) for the
   current task only.

## Where Things Live

| Need | Location |
|---|---|
| Current state (done / in progress / blocked) | `state/CURRENT_STATUS.md` |
| Ordered implementation backlog | `state/TASK_QUEUE.md` |
| Key decisions and rationale | `state/DECISIONS_LOG.md` |
| Behavioral rules | `AGENT_RULES.md` |
| Coding conventions | `CODING_STANDARDS.md` |
| Security requirements | `SECURITY_STANDARDS.md` |
| Environment definitions and behavior | `ENVIRONMENT_GUIDE.md` |
| Resume protocol for a new session | `HOW_TO_RESUME.md` |
| Checklists (feature, endpoint, test, bug fix) | `templates/` |
| Tool permissions | `settings.json` |
| Machine setup (toolchain install) | `../scripts/setup-esp32.sh` |
| Firmware projects | `../firmware/` |

## Self-Update Directive (Mandatory)

After each working session, before closing, the agent **must** update `.claude/`
files to reflect newly discovered project-specific knowledge:

- Stack determined → update `CODING_STANDARDS.md` and `SECURITY_STANDARDS.md`
  with stack-specific guidance immediately. (ESP-IDF v6.1 recorded; extend with
  patterns observed in `firmware/` as projects grow.)
- Architecture decided → update this `README.md` and log it in
  `state/DECISIONS_LOG.md`.
- Environment/toolchain changed → update `ENVIRONMENT_GUIDE.md` and
  `scripts/setup-esp32.sh` together so they never drift apart.
- Any project-level context change → update this `README.md`.

Keeping `.claude/` accurate is part of every task, not an optional extra.
Session-end checklist lives in `AGENT_RULES.md` under "Session End".
