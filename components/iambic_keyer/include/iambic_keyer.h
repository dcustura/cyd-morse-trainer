#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IAMBIC_KEYER_MODE_A = 0,
    IAMBIC_KEYER_MODE_B,
    IAMBIC_KEYER_MODE_STRAIGHT,
    IAMBIC_KEYER_MODE_ULTIMATIC,
} iambic_keyer_mode_t;

typedef enum {
    IAMBIC_KEYER_STATE_IDLE = 0,
    IAMBIC_KEYER_STATE_SEND_DIT,
    IAMBIC_KEYER_STATE_SEND_DAH,
    IAMBIC_KEYER_STATE_GAP,
} iambic_keyer_state_t;

/**
 * Iambic paddle keyer state machine (Curtis mode A/B), an Ultimatic mode
 * (a live squeeze repeats whichever paddle was pressed most recently, with
 * no alternation; a paddle tapped and released while the other stays held
 * is still inserted once, but nothing is remembered once both paddles are
 * fully released), plus a straight-key passthrough mode.
 *
 * Pure logic: the caller supplies a monotonically increasing millisecond
 * timestamp on every service() call rather than the keyer reading a clock
 * itself, so it can be driven either by real hardware (esp_timer) or by a
 * synthetic timestamp sequence in tests.
 */
typedef struct {
    iambic_keyer_mode_t mode;
    uint16_t wpm;
    uint32_t unit_ms;
    iambic_keyer_state_t state;
    uint32_t element_end_ms;
    uint32_t gap_end_ms;
    bool opposite_latched; /* opposite paddle touched during the current element+gap: inserted
                             * next if the held paddle is still down (Iambic A/B and Ultimatic),
                             * or once more after a full release (mode B only) */
    bool forced_extra;     /* currently sending the one forced element from opposite_latched (Iambic only) */
    bool sending_dit;      /* which element type the current SEND/GAP state represents */
    bool ultimatic_last_dit;   /* Ultimatic only: which paddle has priority during a live squeeze */
    bool prev_dit_contact;     /* Ultimatic only: previous-tick raw dit level, for rising-edge detection */
    bool prev_dah_contact;     /* Ultimatic only: previous-tick raw dah level, for rising-edge detection */
} iambic_keyer_t;

void iambic_keyer_init(iambic_keyer_t *k, iambic_keyer_mode_t mode, uint16_t wpm);
void iambic_keyer_set_mode(iambic_keyer_t *k, iambic_keyer_mode_t mode);
void iambic_keyer_set_wpm(iambic_keyer_t *k, uint16_t wpm);

/**
 * Advance the state machine by one tick (call at a fixed rate, 1ms
 * recommended) with the current debounced, logical (post-polarity-swap)
 * paddle contact states. Returns the key-down/sidetone output for this
 * tick: true while an element (dit or dah) is being sent.
 *
 * In IAMBIC_KEYER_MODE_STRAIGHT, dit_contact is returned directly and
 * dah_contact is ignored entirely.
 */
bool iambic_keyer_service(iambic_keyer_t *k, bool dit_contact, bool dah_contact, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
