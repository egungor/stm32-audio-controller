# Tests

Cross-component tests live here. Tests that belong to one component live
beside it:

| Tests | Location |
|---|---|
| Protocol codec | `protocol/tests` |
| STM32 protocol client | `firmware/link/tests` |
| Host core | `host/Tests` |
| UI (screenshots, no assertions) | `firmware/simulator` |
| Firmware ↔ host loopback | [`integration/`](integration/README.md) |

Each has a CI workflow in `.github/workflows/`.
