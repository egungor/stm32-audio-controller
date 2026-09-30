# Development Workflow

## Principle

Use AI for implementation speed while keeping architectural and hardware
decisions under human control.

## Standard flow

```text
Requirement
    ↓
Issue
    ↓
Architect / Plan
    ↓
Implementation
    ↓
Build
    ↓
Tests
    ↓
Review
    ↓
Hardware validation (if required)
    ↓
PR
    ↓
Merge
```

## Issue format

Every implementation issue should contain:

### Goal
What should change?

### Context
Why is it needed?

### Constraints
What must not change?

### Acceptance criteria
How do we know it is complete?

### Verification
Which automated and manual checks are required?

## Example

```text
Goal:
Implement STM32 HID transmit transport.

Constraints:
- USB HID transport only.
- Do not block the UI.
- Do not modify generated code unnecessarily.

Acceptance criteria:
- Firmware builds.
- Host receives a test packet.
- USB disconnect does not crash firmware.

Hardware verification:
- Flash board.
- Connect USB.
- Confirm packet reception.
```

## Human approval points

The human developer should approve:

- architecture changes
- protocol changes
- new external dependencies
- hardware assumptions
- security-sensitive Windows behavior
- final PR merge

## AI reporting

Every AI task completion should state:

1. What changed
2. Which files changed
3. What was built
4. Which tests were run
5. Which hardware checks were not performed
6. Any known risks
