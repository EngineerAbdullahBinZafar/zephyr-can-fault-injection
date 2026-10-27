"""
can_fault_injector.py
Zephyr RTOS CAN Controller Bus-Off & Interrupt Flood Test Harness
Author: Engineer Abdullah Bin Zafar
Target: NXP FlexCAN (S32K3 / i.MX RT), STM32 bxCAN/FDCAN
License: Apache-2.0
"""

import time
import enum
from typing import Optional, Tuple

class CANState(enum.Enum):
    ERROR_ACTIVE = 1
    ERROR_WARNING = 2
    ERROR_PASSIVE = 3
    BUS_OFF = 4
    STOPPED = 5
    STARTING = 6

class TokenBucketRateLimiter:
    """
    Deterministic Token-Bucket Governor for In-ISR / Near-ISR Logging.
    Prevents log buffer saturation and interrupt jitter degradation.
    """
    def __init__(self, capacity: int = 10, refill_rate_per_sec: float = 5.0):
        self.capacity = capacity
        self.tokens = float(capacity)
        self.refill_rate = refill_rate_per_sec
        self.last_update = time.time()
        self.dropped_logs_count = 0

    def allow_log(self, current_time: Optional[float] = None) -> Tuple[bool, int]:
        now = time.time() if current_time is None else current_time
        delta = max(0.0, now - self.last_update)
        self.last_update = now

        self.tokens = min(float(self.capacity), self.tokens + delta * self.refill_rate)

        if self.tokens >= 1.0:
            self.tokens -= 1.0
            dropped = self.dropped_logs_count
            self.dropped_logs_count = 0
            return (True, dropped)
        else:
            self.dropped_logs_count += 1
            return (False, self.dropped_logs_count)

class SimulatedCANController:
    """
    Simulates ISO 11898-1:2015 CAN controller state machine under physical faults.
    """
    def __init__(self, name: str = "FlexCAN_0"):
        self.name = name
        self.tec = 0  # Transmit Error Counter
        self.rec = 0  # Receive Error Counter
        self.state = CANState.ERROR_ACTIVE
        self.log_ring_buffer_size = 256
        self.log_buffer_occupancy = 0
        self.total_interrupts_fired = 0
        self.total_logs_emitted = 0
        self.total_logs_dropped = 0
        self.rate_limiter = TokenBucketRateLimiter(capacity=10, refill_rate_per_sec=20.0)

    def trigger_transmission_error(self, is_tx: bool = True, delta: int = 8):
        """Simulates an arbitration / ACK / CRC bit error on bus."""
        self.total_interrupts_fired += 1
        if is_tx:
            self.tec += delta
        else:
            self.rec += 1

        self._update_state()

    def _update_state(self):
        # Per ISO 11898-1: TEC > 255 triggers Bus-Off
        if self.tec > 255:
            self.state = CANState.BUS_OFF
        elif self.tec > 127 or self.rec > 127:
            self.state = CANState.ERROR_PASSIVE
        elif self.tec > 96 or self.rec > 96:
            self.state = CANState.ERROR_WARNING
        else:
            self.state = CANState.ERROR_ACTIVE

    def handle_errint_hardened(self, sim_time: float) -> Tuple[bool, int]:
        allowed, dropped = self.rate_limiter.allow_log(sim_time)
        if allowed:
            self.total_logs_emitted += 1
        return (allowed, dropped)

    def recover_bus_off(self, consecutive_recessive_bursts: int) -> bool:
        """
        ISO 11898-1 auto-recovery: requires 128 sequences of 11 consecutive recessive bits.
        """
        if self.state == CANState.BUS_OFF:
            if consecutive_recessive_bursts >= 128:
                self.tec = 0
                self.rec = 0
                self.state = CANState.ERROR_ACTIVE
                return True
        return False
