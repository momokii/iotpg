## Task Queue

> Stack confirmed: ESP-IDF v6.1, CLI-only. Backlog below — agent works top-down.

| Task ID | Name | Priority | Status | Complexity | Depends On | Scope | Acceptance Criteria | Security Concerns |
|---|---|---|---|---|---|---|---|---|
| TASK-001 | Verify flash on real ESP32 board | High | DONE | S | — | Plug board, find port, `idf.py -p <PORT> flash monitor` hello_world | "Hello world!" + chip info in monitor output | Confirm user in `dialout`; never `erase-flash` a non-dev board without asking |
| TASK-002 | First firmware: GPIO blink in `firmware/blink/` | High | IN PROGRESS (lamp dark on GPIO5; pin_sweep running to find lamp pin) | S | TASK-001 | IDF blink example adapted to onboard LED pin of user's board | LED blinks, build clean, runs after power cycle | None (no network, no secrets) |
| TASK-003 | Wi-Fi station + MQTT telemetry sketch | Medium | TODO | M | TASK-002 | Connect to AP, publish sensor/hello message to broker | Messages arrive at broker; reconnects on AP drop | Secrets via Kconfig dev-only / NVS for real; validate broker TLS before sending anything sensitive |

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
