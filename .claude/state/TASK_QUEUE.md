## Task Queue

> Stack confirmed: ESP-IDF v6.1, CLI-only. Backlog below — agent works top-down.

| Task ID | Name | Priority | Status | Complexity | Depends On | Scope | Acceptance Criteria | Security Concerns |
|---|---|---|---|---|---|---|---|---|
| TASK-001 | Verify flash on real ESP32 board | High | DONE | S | — | Plug board, find port, `idf.py -p <PORT> flash monitor` hello_world | "Hello world!" + chip info in monitor output | Confirm user in `dialout`; never `erase-flash` a non-dev board without asking |
| TASK-002 | First firmware: GPIO blink in `firmware/blink/` | High | DONE (GPIO2 onboard blue LED, 5 s on/off, user confirmed visible) | S | TASK-001 | IDF blink example adapted to onboard LED pin of user's board | LED blinks, build clean, runs after power cycle | None (no network, no secrets) |
| TASK-003 | Wi-Fi station + MQTT telemetry sketch | Medium | TODO | M | TASK-002 | Connect to AP, publish sensor/hello message to broker | Messages arrive at broker; reconnects on AP drop | Secrets via Kconfig dev-only / NVS for real; validate broker TLS before sending anything sensitive |
| TASK-004 | Traffic-light module (R/Y/G) on GPIO 21/22/23 | High | DONE (direct female-to-female wiring; breadboard was the fault; 2 s cycle confirmed on monitor) | S | TASK-002 | `firmware/traffic`: red→yellow→green 2 s each on module wired R→21 Y→22 G→23 | Lamps cycle visibly; order matches R/Y/G assumption (user to confirm sequence) | None (no network, no secrets) |
| TASK-005 | Button-controlled light shows via onboard BOOT button | High | DONE (5 animated shows, BOOT press switches show, user confirmed working) | M | TASK-004 | `firmware/button_lights`: CHASE/TRAFFIC/BLINK/BUILD-UP/PING-PONG, random on press | Press changes show visibly; no repeat of same show | None (no network, no secrets; GPIO0 strapping caveat documented) |
| TASK-006 | Project docs + progress log (per-project READMEs, firmware index) | Medium | DONE (4 docs written, all 3 projects rebuilt clean) | S | TASK-005 | README per project + `firmware/README.md` tracker | Every project documented and rebuildable | None |

### Template Format (use for every task added)

| Field               | Value                                              |
|---------------------|----------------------------------------------------|
| Task ID             | TASK-001                                           |
| Name                | [Task name]                                        |
| Priority            | High / Medium / Low                                |
| Status              | TODO / IN PROGRESS / DONE / BLOCKED                |
| Complexity          | S / M / L                                          |
| Depends On          | [Task IDs this task requires to be done first]     |
| Scope               | [Exact description of what must be built]          |
| Acceptance Criteria | [What "done" looks like, measurable]               |
| Security Concerns   | [Any security considerations specific to this task]|
