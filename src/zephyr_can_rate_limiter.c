/*
 * zephyr_can_rate_limiter.c
 * Deterministic In-ISR Token-Bucket Log Governor & ISO 11898-1:2015
 * CAN Controller Fault State Machine Implementation
 *
 * Zero dynamic heap allocation. Zero floating-point operations (ISR-safe).
 * Compiles cleanly under -std=c11 -Wall -Wextra -Werror.
 *
 * Author: Engineer Abdullah Bin Zafar
 * License: Apache-2.0
 */

#include "zephyr_can_rate_limiter.h"
#include <stddef.h>
#include <string.h>

#define MICRO_SCALE ((uint64_t)1000000ULL)
#define ISO11898_BUS_OFF_RECESSIVE_GOAL 128U

int can_rate_limiter_init(can_error_rate_limiter_t *limiter,
                          uint32_t capacity_tokens,
                          uint32_t refill_per_sec,
                          int64_t initial_time_ms)
{
    if (limiter == NULL || capacity_tokens == 0U || refill_per_sec == 0U) {
        return -1;
    }

    memset(limiter, 0, sizeof(*limiter));
    limiter->max_micro_tokens = (uint64_t)capacity_tokens * MICRO_SCALE;
    limiter->micro_tokens = limiter->max_micro_tokens;
    /* micro-tokens per ms = (refill_per_sec * 1,000,000) / 1000 = refill_per_sec * 1000 */
    limiter->refill_per_ms_micro = refill_per_sec * 1000U;
    limiter->last_refill_ms = initial_time_ms;
    return 0;
}

can_log_decision_t can_rate_limiter_eval(can_error_rate_limiter_t *limiter,
                                         int64_t now_ms)
{
    can_log_decision_t decision = { false, 0U };

    if (limiter == NULL) {
        return decision;
    }

    if (now_ms > limiter->last_refill_ms) {
        uint64_t delta_ms = (uint64_t)(now_ms - limiter->last_refill_ms);
        uint64_t added = delta_ms * (uint64_t)limiter->refill_per_ms_micro;

        /* Saturating addition bounded by burst ceiling */
        if (added >= limiter->max_micro_tokens ||
            limiter->micro_tokens + added >= limiter->max_micro_tokens) {
            limiter->micro_tokens = limiter->max_micro_tokens;
        } else {
            limiter->micro_tokens += added;
        }
        limiter->last_refill_ms = now_ms;
    }

    if (limiter->micro_tokens >= MICRO_SCALE) {
        limiter->micro_tokens -= MICRO_SCALE;
        decision.emit_log = true;
        decision.newly_reported_drops = limiter->suppressed_count;
        limiter->suppressed_count = 0U;
        limiter->total_allowed++;
    } else {
        decision.emit_log = false;
        limiter->suppressed_count++;
        limiter->total_suppressed++;
        decision.newly_reported_drops = 0U;
    }

    return decision;
}

static void update_iso11898_state(can_fault_ctx_t *ctx)
{
    /* ISO 11898-1:2015 Section 12.1.4 error state thresholds */
    if (ctx->tec > 255U) {
        ctx->state = CAN_CTRL_STATE_BUS_OFF;
    } else if (ctx->tec > 127U || ctx->rec > 127U) {
        ctx->state = CAN_CTRL_STATE_ERROR_PASSIVE;
    } else if (ctx->tec > 96U || ctx->rec > 96U) {
        ctx->state = CAN_CTRL_STATE_ERROR_WARNING;
    } else {
        ctx->state = CAN_CTRL_STATE_ERROR_ACTIVE;
    }
}

int can_fault_ctx_init(can_fault_ctx_t *ctx,
                       uint32_t log_capacity,
                       uint32_t log_refill_per_sec,
                       int64_t initial_time_ms)
{
    if (ctx == NULL) {
        return -1;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->state = CAN_CTRL_STATE_ERROR_ACTIVE;
    return can_rate_limiter_init(&ctx->limiter,
                                 log_capacity,
                                 log_refill_per_sec,
                                 initial_time_ms);
}

can_log_decision_t can_fault_inject_error(can_fault_ctx_t *ctx,
                                          bool is_tx,
                                          int64_t now_ms)
{
    can_log_decision_t decision = { false, 0U };
    if (ctx == NULL || ctx->state == CAN_CTRL_STATE_STOPPED) {
        return decision;
    }

    ctx->total_errint_fires++;

    if (is_tx) {
        if (ctx->tec <= 255U) {
            ctx->tec += 8U;
        }
    } else {
        if (ctx->rec < 255U) {
            ctx->rec += 1U;
        }
    }

    update_iso11898_state(ctx);
    return can_rate_limiter_eval(&ctx->limiter, now_ms);
}

bool can_fault_attempt_bus_off_recovery(can_fault_ctx_t *ctx,
                                        uint32_t recessive_seq_count)
{
    if (ctx == NULL || ctx->state != CAN_CTRL_STATE_BUS_OFF) {
        return false;
    }

    ctx->recovery_recessive_seqs += recessive_seq_count;
    if (ctx->recovery_recessive_seqs >= ISO11898_BUS_OFF_RECESSIVE_GOAL) {
        ctx->tec = 0U;
        ctx->rec = 0U;
        ctx->recovery_recessive_seqs = 0U;
        ctx->state = CAN_CTRL_STATE_ERROR_ACTIVE;
        return true;
    }
    return false;
}

const char *can_ctrl_state_str(can_ctrl_state_t state)
{
    switch (state) {
        case CAN_CTRL_STATE_ERROR_ACTIVE:  return "ERROR_ACTIVE";
        case CAN_CTRL_STATE_ERROR_WARNING: return "ERROR_WARNING";
        case CAN_CTRL_STATE_ERROR_PASSIVE: return "ERROR_PASSIVE";
        case CAN_CTRL_STATE_BUS_OFF:       return "BUS_OFF";
        case CAN_CTRL_STATE_STOPPED:       return "STOPPED";
        default:                           return "UNKNOWN";
    }
}
