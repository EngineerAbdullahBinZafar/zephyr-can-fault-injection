/*
 * test_zephyr_can_c.c
 * Standalone C11 Unit Test Suite for Zephyr CAN Rate Limiter & Bus-Off Recovery
 * Compiled with -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined
 */

#include "zephyr_can_rate_limiter.h"
#include <assert.h>
#include <stdio.h>

static void test_initial_burst_and_suppression(void)
{
    can_error_rate_limiter_t limiter;
    assert(can_rate_limiter_init(&limiter, 10U, 5U, 1000) == 0);

    /* Consume burst of 10 tokens at identical timestamp (0 ms delta) */
    for (int i = 0; i < 10; i++) {
        can_log_decision_t d = can_rate_limiter_eval(&limiter, 1000);
        assert(d.emit_log == true);
        assert(d.newly_reported_drops == 0U);
    }

    /* Next 25 immediate ERRINT fires must be clamped without log spam */
    for (int i = 0; i < 25; i++) {
        can_log_decision_t d = can_rate_limiter_eval(&limiter, 1000);
        assert(d.emit_log == false);
    }

    assert(limiter.total_allowed == 10ULL);
    assert(limiter.total_suppressed == 25ULL);
    assert(limiter.suppressed_count == 25U);
    printf("  [PASS] test_initial_burst_and_suppression\n");
}

static void test_deterministic_token_regeneration(void)
{
    can_error_rate_limiter_t limiter;
    /* Capacity 2, Refill 10 tokens/sec => 1 token every 100 ms */
    assert(can_rate_limiter_init(&limiter, 2U, 10U, 0) == 0);

    assert(can_rate_limiter_eval(&limiter, 0).emit_log == true);
    assert(can_rate_limiter_eval(&limiter, 0).emit_log == true);
    assert(can_rate_limiter_eval(&limiter, 0).emit_log == false);

    /* Advance 50 ms => 0.5 tokens (not yet 1 full log) */
    assert(can_rate_limiter_eval(&limiter, 50).emit_log == false);

    /* Advance another 60 ms (total 110 ms => 1.1 tokens) => flushes 2 dropped logs */
    can_log_decision_t recovered = can_rate_limiter_eval(&limiter, 110);
    assert(recovered.emit_log == true);
    assert(recovered.newly_reported_drops == 2U);
    printf("  [PASS] test_deterministic_token_regeneration\n");
}

static void test_iso11898_state_machine_and_bus_off_recovery(void)
{
    can_fault_ctx_t ctx;
    assert(can_fault_ctx_init(&ctx, 10U, 20U, 0) == 0);
    assert(ctx.state == CAN_CTRL_STATE_ERROR_ACTIVE);

    /* 12 TX errors * 8 = 96 -> still ERROR_ACTIVE */
    for (int i = 0; i < 12; i++) {
        can_fault_inject_error(&ctx, true, i);
    }
    assert(ctx.tec == 96U);
    assert(ctx.state == CAN_CTRL_STATE_ERROR_ACTIVE);

    /* 13th TX error -> TEC = 104 (> 96) -> ERROR_WARNING */
    can_fault_inject_error(&ctx, true, 13);
    assert(ctx.state == CAN_CTRL_STATE_ERROR_WARNING);

    /* Push past 127 -> ERROR_PASSIVE */
    for (int i = 0; i < 3; i++) {
        can_fault_inject_error(&ctx, true, 20 + i);
    }
    assert(ctx.tec == 128U);
    assert(ctx.state == CAN_CTRL_STATE_ERROR_PASSIVE);

    /* Push past 255 -> BUS_OFF */
    for (int i = 0; i < 16; i++) {
        can_fault_inject_error(&ctx, true, 30 + i);
    }
    assert(ctx.tec == 256U);
    assert(ctx.state == CAN_CTRL_STATE_BUS_OFF);

    /* Recovery requires >= 128 sequences of 11 recessive bits */
    assert(can_fault_attempt_bus_off_recovery(&ctx, 100U) == false);
    assert(ctx.state == CAN_CTRL_STATE_BUS_OFF);

    assert(can_fault_attempt_bus_off_recovery(&ctx, 28U) == true);
    assert(ctx.state == CAN_CTRL_STATE_ERROR_ACTIVE);
    assert(ctx.tec == 0U && ctx.rec == 0U);
    printf("  [PASS] test_iso11898_state_machine_and_bus_off_recovery\n");
}

static void test_10khz_errint_storm_mitigation(void)
{
    can_fault_ctx_t ctx;
    /* 10 burst capacity, 20 logs/sec = 1 log per 50 ms */
    assert(can_fault_ctx_init(&ctx, 10U, 20U, 0) == 0);

    /* Simulate 1,000 ERRINT interrupts over 100 ms (10 kHz storm: 10 interrupts/ms) */
    for (int i = 0; i < 1000; i++) {
        int64_t ts_ms = i / 10;
        can_fault_inject_error(&ctx, true, ts_ms);
    }

    /* Ring buffer ceiling 256 must never be breached: max ~12 logs emitted over 100ms */
    assert(ctx.total_errint_fires == 1000ULL);
    assert(ctx.limiter.total_allowed <= 15ULL);
    assert(ctx.limiter.total_suppressed >= 985ULL);
    assert(ctx.limiter.total_allowed + ctx.limiter.total_suppressed == 1000ULL);
    printf("  [PASS] test_10khz_errint_storm_mitigation (allowed=%llu, suppressed=%llu)\n",
           (unsigned long long)ctx.limiter.total_allowed,
           (unsigned long long)ctx.limiter.total_suppressed);
}

static void test_invalid_inputs_safe(void)
{
    can_error_rate_limiter_t limiter;
    assert(can_rate_limiter_init(NULL, 10U, 5U, 0) == -1);
    assert(can_rate_limiter_init(&limiter, 0U, 5U, 0) == -1);
    assert(can_rate_limiter_init(&limiter, 10U, 0U, 0) == -1);
    assert(can_rate_limiter_eval(NULL, 0).emit_log == false);
    assert(can_fault_ctx_init(NULL, 10U, 5U, 0) == -1);
    assert(can_fault_attempt_bus_off_recovery(NULL, 128U) == false);
    printf("  [PASS] test_invalid_inputs_safe\n");
}

int main(void)
{
    printf("Running Zephyr CAN C11 Unit Test Suite...\n");
    test_initial_burst_and_suppression();
    test_deterministic_token_regeneration();
    test_iso11898_state_machine_and_bus_off_recovery();
    test_10khz_errint_storm_mitigation();
    test_invalid_inputs_safe();
    printf("ALL 5 C11 UNIT TESTS PASSED.\n");
    return 0;
}
