#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Decides whether the external key output (the line wired to a
 * transceiver) may follow the logical key state. Off-air, the output is
 * never driven; the sidetone and decoder are unaffected. Going on-air
 * while the key is already down doesn't key the rest of that element:
 * the output waits for the next key-down, so it never starts mid-element.
 * Going off-air during an element releases the output at once.
 */
typedef struct {
    bool output;        /* current output state */
    bool prev_key_down; /* key state at the previous update */
} key_gate_t;

void key_gate_init(key_gate_t *g);

/** Feed the current key state and on-air state; returns whether the output is on. */
bool key_gate_update(key_gate_t *g, bool key_down, bool on_air);

#ifdef __cplusplus
}
#endif
