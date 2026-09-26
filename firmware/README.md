# Firmware projects — progress log

Every project built on this board lives here: one directory, one IDF project,
one README. Read the project README for wiring, behavior, and flash commands.

| # | Project | What it does | Hardware | Status |
|---|---|---|---|---|
| 01 | [`blink/`](blink/) | Onboard blue LED, 5 s on/off | None (onboard LED) | DONE (TASK-002) |
| 02 | [`traffic/`](traffic/) | R/Y/G static sequence, 2 s each | R/Y/G module → GPIO 21/22/23 + GND | DONE (TASK-004) |
| 03 | [`button_lights/`](button_lights/) | 5 animated shows, BOOT switches | Same module + onboard BOOT button | DONE (TASK-005) |
| 04 | [`env_display/`](env_display/) | DHT11 temp/humidity on auto-discovered LCD/OLED | DHT + display modules | DONE (TASK-007) |
| — | [`pin_sweep/`](pin_sweep/) | Dev tool: sweeps GPIOs to find unknown lamp pins | Any lamp circuit | Tool (kept for reuse) |

## How to work on a project

```bash
get_idf                                        # activate ESP-IDF v6.1 env
idf.py -C firmware/<name> set-target esp32    # once per fresh checkout
idf.py -C firmware/<name> build
idf.py -C firmware/<name> -p /dev/ttyUSB0 flash monitor
```

Only one project runs at a time — flashing replaces whatever was on the board.
Re-flash any project above to revisit it; each README documents its wiring.

## Conventions for new projects

- Copy the smallest existing project as a skeleton (or `$IDF_PATH/examples/get-started/hello_world`).
- Document wiring + behavior in the project README **before** closing the task.
- Never commit `build/`, `sdkconfig`, `sdkconfig.old`, `managed_components/`
  (all gitignored). Commit `sdkconfig.defaults*` and `dependencies.lock`.
- Log the task in `.claude/state/TASK_QUEUE.md` and decisions in `DECISIONS_LOG.md`.
