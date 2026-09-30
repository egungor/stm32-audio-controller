# Agent Rules

These rules apply to all AI agents working in this repository.

## Before editing

- Read `CLAUDE.md`.
- Read the relevant architecture/protocol documentation.
- Inspect existing code before creating new abstractions.
- Identify whether the task affects firmware, host, protocol, or multiple components.

## Ownership

- `firmware/` → STM32 firmware agent
- `host/` → Windows host agent
- `protocol/` → protocol agent
- `docs/` → architect/documentation work
- `.github/` → repository automation

## Cross-component changes

If a change affects both firmware and host:

1. Define/update the protocol first.
2. Add or update protocol tests.
3. Implement host and firmware sides.
4. Run all available automated checks.
5. Explicitly identify hardware tests that remain.

## Do not

- Invent hardware specifications.
- Invent test results.
- silently change protocol packet formats.
- make broad refactors unrelated to the task.
- overwrite user work without inspecting it first.
