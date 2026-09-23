# Coding Standards — Stack-Agnostic Starting Point

> **Self-update instruction:** When the tech stack is confirmed, replace this
> file's general rules with language/framework-specific conventions, linting
> config references, and actual patterns observed in the codebase. This file
> starts general by design and must evolve.

## General Principles

- Clarity over cleverness — code is read more than it is written.
- One responsibility per function, file, and module.
- Explicit is better than implicit.
- Fail fast and loudly — surface errors at the earliest possible point.

## Naming Conventions (General)

- Files: `kebab-case` for most ecosystems; follow the established pattern once
  determined.
- Functions/methods: descriptive verb-noun pairs (`getUserById`, `validateInput`).
- Constants: `UPPER_SNAKE_CASE`.
- Boolean variables: prefix with `is`, `has`, `should`, `can`.

## Error Handling

- Never silently swallow errors.
- All errors must be logged with sufficient context to reproduce.
- User-facing errors must never expose internal stack traces or system details.

## Testing

- Every new feature must include at least one test.
- Every bug fix must include a regression test.
- Tests must be runnable with a single command.

## Documentation

- Every public function must have a descriptive comment or docstring.
- Every non-obvious decision in code must have an inline comment explaining *why*.

## Git Workflow & Commit Messages

All commits follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <short imperative summary>

[Optional body — what changed and why, not how.]
[Optional footer — e.g. Refs: TASK-001]
```

- **Types:** `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`,
  `ci`, `revert`. Use `feat!` / `BREAKING CHANGE:` footer for breaking changes.
- **Summary line:** imperative mood (`add`, not `added`), ≤ 72 chars, no trailing
  period. Example: `feat(auth): add JWT refresh token rotation`.
- **One logical change per commit** — never mix a feature with an unrelated
  refactor or drive-by fix.
- **Branch naming:** `<type>/<task-id>-short-slug`, e.g.
  `feat/TASK-001-jwt-refresh`, `fix/TASK-014-null-guard`.
- **Before every commit:** run the linter and the full test suite; a commit that
  breaks either is not pushed.
- **Never commit:** secrets, `.env` files, local-only config, generated build
  output, or commented-out code blocks.
- Reference the task ID (`TASK-XXX`) in the body so every commit traces back to
  `state/TASK_QUEUE.md`.

## Stack Notes — ESP-IDF v6.1 Firmware (confirmed 2026-09-24)

Extends the general rules above for `firmware/` projects. Expand with observed
patterns as projects grow.

- **Workflow is CLI-only:** `get_idf` → `idf.py set-target esp32` → `idf.py build`
  → `idf.py -p <PORT> flash monitor`. No IDE project files — never commit any.
- **Project layout:** one IDF project per `firmware/<name>/` directory
  (`CMakeLists.txt` + `main/` + `main/CMakeLists.txt`). Copy from
  `$IDF_PATH/examples/get-started/hello_world` to start.
- **Generated files are never committed:** `build/`, `sdkconfig`, `sdkconfig.old`
  (all gitignored). Commit `sdkconfig.defaults` instead when custom config is needed.
- **Error handling:** check every `esp_err_t` return with `ESP_ERROR_CHECK` during
  init; handle runtime errors explicitly — never ignore them silently.
- **Logging:** use `ESP_LOGx` tags per module (`static const char *TAG = "..."`).
  Log levels via `menuconfig` → Component config → Log output. No credentials
  or keys in logs, ever.
- **Wi-Fi credentials and secrets:** via `menuconfig`/Kconfig defaults for dev
  only, real secrets via NVS or env-provisioned partition — never hardcoded.
- **Test on hardware:** `idf.py -p <PORT> flash monitor` is the firmware
  equivalent of the test suite — a task is not DONE until it runs on the board
  (or the board-absent reason is logged in `CURRENT_STATUS.md`).
