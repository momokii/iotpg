## Decisions Log

> No decisions logged yet. This file must be updated by the agent whenever a
> significant decision is made during a working session.

### Template Format

---
**Decision:** [What was decided]
**Date:** [YYYY-MM-DD]
**Context:** [Why this decision was needed]
**Rationale:** [Why this option was chosen]
**Alternatives Rejected:** [Other options considered and why they were not chosen]
**Security Implications:** [Any security impact of this decision]
**Impact:** [What this decision affects downstream]
---

## Entries

### 2026-09-24 — ESP32 toolchain: ESP-IDF v6.1, CLI-only

- **Decision:** Use Espressif's official ESP-IDF v6.1 directly (idf.py/export.sh),
  no IDE, no Arduino wrapper, no PlatformIO.
- **Context:** User starts ESP32 play, asked whether a code editor is needed and
  whether Espressif tooling covers the workflow; machine has no sudo.
- **Rationale:** ESP-IDF is CLI-native by design; v6.1 is latest stable
  (2026-08-27). Arduino-ESP32 3.3.x lags on IDF 5.5; PlatformIO wraps/freezes
  toolchain versions. No editor needed — build/flash/monitor all terminal.
- **Alternatives Rejected:** Arduino-ESP32 (older IDF base, 4.0 still RC);
  PlatformIO Core (version-lag indirection, buys nothing for single-vendor ESP32).
- **Security Implications:** Toolchain from official Espressif sources only
  (github.com/espressif, dl.espressif.com); Ubuntu .debs from official archive.
- **Impact:** All firmware work uses idf.py; `scripts/setup-esp32.sh` is the
  single source of truth for machine setup.

### 2026-09-24 — No-sudo user-space install path

- **Decision:** On machines without sudo, extract Ubuntu .debs (cmake, ninja,
  flex, bison, gperf, ccache, dfu-util) into `~/.local/esp-sysroot` via
  `dpkg-deb -x` instead of requiring root.
- **Context:** This machine has no passwordless sudo; ESP-IDF build needs
  cmake/ninja/flex/bison/gperf.
- **Rationale:** Zero-root install, fully reversible (`rm -rf ~/.local/esp-sysroot`),
  scripted in `setup-esp32.sh` with a sudo fast-path when available.
- **Alternatives Rejected:** pip cmake/ninja only (leaves flex/bison/gperf gaps);
  asking user for password mid-run (breaks unattended setup).
- **Security Implications:** .debs come from the official Ubuntu archive; no
  third-party binaries. Verified working: full hello_world build.
- **Impact:** `setup-esp32.sh` works on locked-down machines and fresh VMs alike.

### 2026-09-22 — Initialize `.claude/` agent infrastructure

- **Decision:** Bootstrap a general, stack-agnostic `.claude/` infrastructure
  before any product code is written.
- **Context:** Blank repository developed via iterative AI-assisted coding; agents
  need orientation without manual re-briefing.
- **Rationale:** General-first docs that self-update each session keep every agent
  oriented from day one and converge to project-specific accuracy over time.
- **Alternatives Rejected:** Stack-specific setup now — rejected because no stack
  is known yet; guessing would create false precision.
- **Security Implications:** None yet — security standards will be enforced from
  the first implementation commit.
- **Impact:** All future sessions follow `HOW_TO_RESUME.md` and the session
  start/end rules in `AGENT_RULES.md`.
