# Contributing to Zephyr CAN Rate Limiter

Thank you for helping improve deterministic CAN subsystem reliability.

## Getting Started

1. Clone and run both verification suites locally:
   ```bash
   make test          # C11 driver + AddressSanitizer/UBSan unit tests
   pytest tests/ -v   # Python ISO 11898-1 state machine simulation suite
   ```
2. Maintain strict **zero-heap, zero-FPU** constraints inside `src/zephyr_can_rate_limiter.c` so functions remain 100% ISR-safe across ARM Cortex-M and NXP S32K targets.
3. Ensure compilation passes cleanly with `-std=c11 -Wall -Wextra -Werror -Wpedantic`.

## Opening Pull Requests
- Cite the relevant ISO 11898-1:2015 section or Zephyr `subsys/can` subsystem issue.
- Include reproduction vectors in `tests/test_zephyr_can_c.c` for any state machine changes.
