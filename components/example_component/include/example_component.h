#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * State for a consecutive-sample debounce filter.
 *
 * Holds `state` until `threshold` consecutive samples disagree with it, at
 * which point it flips. Any sample that agrees with the current state resets
 * the run, so short bounces back to the current state are absorbed.
 */
typedef struct {
    uint8_t threshold;
    uint8_t counter;
    bool state;
} example_component_debounce_t;

/**
 * Initialize a debounce filter.
 *
 * @param db            Filter to initialize.
 * @param threshold     Number of consecutive disagreeing samples required to
 *                       flip the state. A value of 0 is treated as 1.
 * @param initial_state Starting output state.
 */
void example_component_debounce_init(example_component_debounce_t *db, uint8_t threshold, bool initial_state);

/**
 * Feed one sample into the filter and get the (possibly updated) state.
 *
 * @param db     Filter previously initialized with example_component_debounce_init().
 * @param sample Raw input sample for this step.
 * @return The debounced output state after processing this sample.
 */
bool example_component_debounce_feed(example_component_debounce_t *db, bool sample);

#ifdef __cplusplus
}
#endif
