/*
 * zephyr_can_rate_limiter.h
 * Deterministic Token-Bucket Error Interrupt Log Rate Limiter & ISO 11898-1
 * Bus-Off Recovery State Machine for Zephyr RTOS CAN Drivers
 *
 * Target Hardware: NXP FlexCAN (S32K3 / i.MX RT), STM32 FDCAN/bxCAN
 * Standard: ISO 11898-1:2015 | C11 Bare-Metal / Zephyr RTOS Subsys Compatible
 * Author: Engineer Abdullah Bin Zafar
 * License: Apache-2.0
 */

#ifndef ZEPHYR_CAN_RATE_LIMITER_H_
#define ZEPHYR_CAN_RATE_LIMITER_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ISO 11898-1:2015 CAN bus controller error states.
 */
typedef enum {
    CAN_CTRL_STATE_ERROR_ACTIVE = 0,  /**< TEC <= 96 and REC <= 96 */
    CAN_CTRL_STATE_ERROR_WARNING,     /**< TEC > 96 or REC > 96 */
    CAN_CTRL_STATE_ERROR_PASSIVE,     /**< TEC > 127 or REC > 127 */
    CAN_CTRL_STATE_BUS_OFF,           /**< TEC > 255 */
    CAN_CTRL_STATE_STOPPED            /**< Controller halted safely */
} can_ctrl_state_t;

/**
 * @brief In-ISR lock-free integer token-bucket rate limiter.
 * Uses scaled fixed-point micro-tokens (1 token = 1,000,000 units)
 * to preserve determinism without floating-point unit (FPU) state saving in ISRs.
 */
typedef struct {
    uint64_t micro_tokens;          /**< Available tokens * 1,000,000 */
    uint64_t max_micro_tokens;      /**< Burst capacity * 1,000,000 */
    uint32_t refill_per_ms_micro;   /**< Micro-tokens restored per millisecond */
    int64_t  last_refill_ms;        /**< Timestamp from k_uptime_get() (ms) */
    uint32_t suppressed_count;      /**< Logs suppressed since last emission */
    uint64_t total_allowed;         /**< Total diagnostics emitted */
    uint64_t total_suppressed;      /**< Lifetime suppressed interrupt storms */
} can_error_rate_limiter_t;

/**
 * @brief Decision result emitted on ISR log evaluation.
 */
typedef struct {
    bool     emit_log;              /**< True if caller may safely push to log ringbuf */
    uint32_t newly_reported_drops;  /**< Non-zero summary count if burst ended */
} can_log_decision_t;

/**
 * @brief Hardware controller fault state & diagnostics record.
 */
typedef struct {
    can_ctrl_state_t         state;
    uint16_t                 tec;                  /**< Transmit Error Counter (0..256+) */
    uint16_t                 rec;                  /**< Receive Error Counter (0..255) */
    uint64_t                 total_errint_fires;   /**< Total physical ERRINT IRQs */
    uint32_t                 recovery_recessive_seqs; /**< Consecutive 11-bit recessive seqs */
    can_error_rate_limiter_t limiter;
} can_fault_ctx_t;

/**
 * @brief Initialize token-bucket rate limiter without heap allocation.
 * @param limiter          Pointer to pre-allocated limiter struct.
 * @param capacity_tokens  Burst ceiling (e.g., 10 logs).
 * @param refill_per_sec   Sustained logs/sec (e.g., 20 logs/s).
 * @param initial_time_ms  Current monotonic kernel uptime in ms.
 * @return 0 on success, -1 on invalid arguments.
 */
int can_rate_limiter_init(can_error_rate_limiter_t *limiter,
                          uint32_t capacity_tokens,
                          uint32_t refill_per_sec,
                          int64_t initial_time_ms);

/**
 * @brief Evaluate O(1) ISR-safe log admission decision.
 * Uses pure integer arithmetic (zero FPU, zero dynamic allocation).
 * @param limiter  Limiter context.
 * @param now_ms   Monotonic timestamp in ms.
 * @return Admission flag and any flushed suppression summary counter.
 */
can_log_decision_t can_rate_limiter_eval(can_error_rate_limiter_t *limiter,
                                         int64_t now_ms);

/**
 * @brief Initialize simulated/hardware CAN fault diagnostics context.
 */
int can_fault_ctx_init(can_fault_ctx_t *ctx,
                       uint32_t log_capacity,
                       uint32_t log_refill_per_sec,
                       int64_t initial_time_ms);

/**
 * @brief Advance ISO 11898-1 error counters and state machine upon bus fault.
 * Per ISO 11898-1:2015 Section 12.1.4.2, TX bit/ACK error increments TEC by 8.
 * @param ctx    Fault controller context.
 * @param is_tx  True for TX fault (+8 TEC), false for RX fault (+1 REC).
 * @param now_ms Monotonic timestamp for ISR log throttling evaluation.
 * @return Log admission decision under fault burst.
 */
can_log_decision_t can_fault_inject_error(can_fault_ctx_t *ctx,
                                          bool is_tx,
                                          int64_t now_ms);

/**
 * @brief Attempt deterministic ISO 11898-1 Bus-Off auto-recovery.
 * Requires >= 128 sequences of 11 consecutive recessive bits on bus.
 * @param ctx                 Fault controller context.
 * @param recessive_seq_count Observed 11-bit recessive sequence count.
 * @return true if bus transitioned back to ERROR_ACTIVE, false otherwise.
 */
bool can_fault_attempt_bus_off_recovery(can_fault_ctx_t *ctx,
                                        uint32_t recessive_seq_count);

/**
 * @brief Convert controller error state to human-readable string.
 */
const char *can_ctrl_state_str(can_ctrl_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_CAN_RATE_LIMITER_H_ */
