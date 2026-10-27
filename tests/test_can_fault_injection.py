"""
test_can_fault_injection.py
Unit & Benchmark Suite for CAN Fault Injection & Token-Bucket Rate Limiter
Author: Engineer Abdullah Bin Zafar
"""

import pytest
from can_fault_injector import SimulatedCANController, TokenBucketRateLimiter, CANState

def test_token_bucket_initial_burst():
    limiter = TokenBucketRateLimiter(capacity=5, refill_rate_per_sec=10.0)
    # 5 tokens available initially
    for _ in range(5):
        allowed, dropped = limiter.allow_log(current_time=0.0)
        assert allowed is True
        assert dropped == 0
    # 6th immediate log must be throttled
    allowed, dropped = limiter.allow_log(current_time=0.0)
    assert allowed is False
    assert dropped == 1

def test_token_bucket_regeneration():
    limiter = TokenBucketRateLimiter(capacity=5, refill_rate_per_sec=10.0)
    # Exhaust tokens
    for _ in range(5):
        limiter.allow_log(current_time=0.0)
    
    # Fast-forward 0.5 seconds -> should regenerate 5 tokens (0.5s * 10/s)
    allowed, dropped = limiter.allow_log(current_time=0.5)
    assert allowed is True
    # The dropped counter reported previous suppression
    assert dropped >= 0

def test_can_controller_state_progression():
    controller = SimulatedCANController()
    assert controller.state == CANState.ERROR_ACTIVE

    # Trigger errors to cross ERROR_WARNING (TEC > 96)
    for _ in range(13):  # 13 * 8 = 104
        controller.trigger_transmission_error(is_tx=True, delta=8)

    assert controller.state == CANState.ERROR_WARNING
    assert controller.tec == 104

    # Cross ERROR_PASSIVE (TEC > 127)
    for _ in range(4):   # +32 = 136
        controller.trigger_transmission_error(is_tx=True, delta=8)

    assert controller.state == CANState.ERROR_PASSIVE

    # Cross BUS_OFF (TEC > 255)
    for _ in range(16):
        controller.trigger_transmission_error(is_tx=True, delta=8)

    assert controller.state == CANState.BUS_OFF

def test_bus_off_recovery_sequence():
    controller = SimulatedCANController()
    for _ in range(35):
        controller.trigger_transmission_error(is_tx=True, delta=8)
    assert controller.state == CANState.BUS_OFF

    # Recovery fails if < 128 recessive bursts
    success = controller.recover_bus_off(consecutive_recessive_bursts=100)
    assert success is False
    assert controller.state == CANState.BUS_OFF

    # Recovery succeeds on >= 128 bursts
    success = controller.recover_bus_off(consecutive_recessive_bursts=128)
    assert success is True
    assert controller.state == CANState.ERROR_ACTIVE
    assert controller.tec == 0

def test_mitigation_of_interrupt_flood():
    """
    Simulates a 1000-error storm burst.
    Verifies that the hardened rate limiter stops ring buffer exhaustion.
    """
    controller = SimulatedCANController()
    t = 0.0
    emitted = 0
    suppressed = 0

    for _ in range(1000):
        allowed, dropped = controller.handle_errint_hardened(sim_time=t)
        if allowed:
            emitted += 1
        else:
            suppressed += 1
        t += 0.0001  # 10 kHz error storm

    # Out of 1000 error events in 100 ms, only bounded tokens are emitted
    assert emitted <= 15
    assert suppressed >= 985
