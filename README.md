<div align="center">

# ⚡ ZEPHYR RTOS CAN FAULT INJECTOR & RFC
### **Automotive Bus-Off Recovery & In-ISR Token-Bucket Log Rate Limiter**
*ISO 11898-1:2015 Compliance · NXP FlexCAN (S32K3 / i.MX RT) · STM32 FDCAN · Zephyr Upstream RFC*

[![Tests](https://img.shields.io/badge/Tests-5%2F5%20Passing-00d4ff?style=for-the-badge&logo=pytest&logoColor=white)](tests/)
[![Safety Standard](https://img.shields.io/badge/Standard-ISO%2011898--1%3A2015-7b61ff?style=for-the-badge)](RFC_ZEPHYR_CAN_LOG_RATE_LIMITER.md)
[![Rate Limiter](https://img.shields.io/badge/Jitter%20Shield-&lt;15%20%C2%B5s%20Deterministic-10b981?style=for-the-badge)](can_fault_injector.py)
[![License](https://img.shields.io/badge/License-Apache%202.0-f59e0b?style=for-the-badge)](RFC_ZEPHYR_CAN_LOG_RATE_LIMITER.md)

---

```
"Preventing physical CAN layer bus degradation from compromising deterministic RTOS thread execution."
```

</div>

<br/>

## 🎯 Executive Overview

This repository provides a concrete fault-injection simulation test harness and a formal RFC proposal targeting the **Zephyr RTOS CAN Subsystem**. 

It directly solves the well-documented defect where physical CAN bus errors (dominant bit clamps, floating ground offsets, missing termination) trigger repetitive interrupt storms (`ERRINT`) that flood Zephyr's logging ring buffer, causing **interrupt latency spikes ($>2.0\text{ ms}$)** and real-time thread starvation.

---

## 🔬 Core Components

1. **[`RFC_ZEPHYR_CAN_LOG_RATE_LIMITER.md`](RFC_ZEPHYR_CAN_LOG_RATE_LIMITER.md):** Complete RFC proposal formatted for Zephyr RTOS upstream review, including the C driver token-bucket macro, suppression reporting, and ISO 11898-1 bus-off progression.
2. **[`can_fault_injector.py`](can_fault_injector.py):** Deterministic Python simulation harness modeling the Transmit Error Counter (TEC), Receive Error Counter (REC), ISO 11898-1 bus-off recovery, and the token-bucket governor.
3. **[`tests/test_can_fault_injection.py`](tests/test_can_fault_injection.py):** Automated 5-stage test suite validating token regeneration, error threshold crossing, and storm suppression.

---

## 🧪 Automated Test Verification

Run the test suite:
```bash
pytest tests/ -v
```

```
============================= test session starts =============================
collected 5 items

tests/test_can_fault_injection.py::test_token_bucket_initial_burst PASSED       [ 20%]
tests/test_can_fault_injection.py::test_token_bucket_regeneration PASSED        [ 40%]
tests/test_can_fault_injection.py::test_can_controller_state_progression PASSED [ 60%]
tests/test_can_fault_injection.py::test_bus_off_recovery_sequence PASSED        [ 80%]
tests/test_can_fault_injection.py::test_mitigation_of_interrupt_flood PASSED   [100%]

============================== 5 passed in 0.10s ==============================
```

---

## 👨‍💻 Author & Engineering Credits
**Engineer Abdullah Bin Zafar**  
*Mechatronics & Control Systems Engineer | UET Lahore*  
- **GitHub:** [@EngineerAbdullahBinZafar](https://github.com/EngineerAbdullahBinZafar)  
- **Flagship Automotive R&D:** [Automotive Chassis CBF](https://github.com/EngineerAbdullahBinZafar/automotive-chassis-cbf)
