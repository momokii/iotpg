# How to Resume — Start-of-Session Protocol

Execute these steps in order at the start of **every** session, before any other
action. Do not skip steps.

```
Step 1: Read .claude/README.md
        → Orient yourself: understand the project, stack, and structure

Step 2: Read .claude/state/CURRENT_STATUS.md
        → Know exactly what is done, in progress, and blocked

Step 3: Read .claude/state/TASK_QUEUE.md
        → Identify the next task and confirm its dependencies are met

Step 4: Read .claude/AGENT_RULES.md
        → Re-internalize all behavioral rules before touching anything

Step 5: Read .claude/CODING_STANDARDS.md
        → Re-internalize all conventions before writing any code

Step 6: Read .claude/SECURITY_STANDARDS.md
        → Re-internalize all security requirements before writing any code

Step 7: Identify the active environment
        → Check APP_ENV or equivalent — consult ENVIRONMENT_GUIDE.md if needed

Step 8: Read task-relevant docs
        → PRD section, architecture doc, API contract, or any doc directly relevant
           to the current task

Step 9: Verify the environment is functional
        → Run the project's health-check or startup command
           (update this step with the real command once it is known)

Step 10: Confirm no regressions
        → Run the existing test suite before writing any new code
           (update this step with the real test command once it is known)

Step 11: Begin the task
        → Implement → test → security review → report → update all .claude/ state files
```

## Quick Pre-Flight Checks (Every Session)

- [ ] `git status` is clean (or you know exactly what is dirty and why)
- [ ] You are on the correct branch for the current task
- [ ] `.env` is gitignored; no secrets staged (`git diff --cached`)
- [ ] Test suite passes before you write new code (Step 10)
