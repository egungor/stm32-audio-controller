# Protocol Tests

`test_protocol.c` checks the C codec against `../protocol.md`: CRC check
value, the documented example frame, round trips of every message, and
rejection of bad magic, version, CRC (every single-bit error), length,
command and payload values. No test framework; the executable returns
non-zero on any failure.

Run on Linux (or any host with CMake and a C compiler):

```text
cmake -S protocol -B build/protocol
cmake --build build/protocol
ctest --test-dir build/protocol --output-on-failure
```

CI runs the same steps with GCC on Linux and MSVC on Windows
(`.github/workflows/protocol-ci.yml`).
