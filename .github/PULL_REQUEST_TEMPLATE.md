## Summary
Describe the improvement, CAN driver fix, or RFC update.

## Checklist
- [ ] C11 code compiles cleanly with `-Wall -Wextra -Werror` (`make test`)
- [ ] Zero heap allocation and zero FPU instructions introduced in ISR paths
- [ ] Python harness `pytest tests/ -v` passes 100%
