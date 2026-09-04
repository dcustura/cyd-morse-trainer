#include "iambic_keyer.h"

static void reset_state(iambic_keyer_t *k)
{
    k->state = IAMBIC_KEYER_STATE_IDLE;
    k->element_end_ms = 0;
    k->gap_end_ms = 0;
    k->opposite_latched = false;
    k->forced_extra = false;
    k->sending_dit = false;
}

void iambic_keyer_init(iambic_keyer_t *k, iambic_keyer_mode_t mode, uint16_t wpm)
{
    k->mode = mode;
    reset_state(k);
    iambic_keyer_set_wpm(k, wpm);
}

void iambic_keyer_set_mode(iambic_keyer_t *k, iambic_keyer_mode_t mode)
{
    k->mode = mode;
    reset_state(k);
}

void iambic_keyer_set_wpm(iambic_keyer_t *k, uint16_t wpm)
{
    if (wpm == 0) {
        wpm = 1;
    }
    k->wpm = wpm;
    k->unit_ms = 1200u / wpm;
    if (k->unit_ms == 0) {
        k->unit_ms = 1;
    }
}

static void start_element(iambic_keyer_t *k, bool dit, uint32_t now_ms)
{
    k->sending_dit = dit;
    k->state = dit ? IAMBIC_KEYER_STATE_SEND_DIT : IAMBIC_KEYER_STATE_SEND_DAH;
    k->element_end_ms = now_ms + (dit ? k->unit_ms : 3u * k->unit_ms);
    k->opposite_latched = false;
}

static void latch_opposite(iambic_keyer_t *k, bool dit_contact, bool dah_contact)
{
    bool opposite_pressed = k->sending_dit ? dah_contact : dit_contact;
    if (opposite_pressed) {
        k->opposite_latched = true;
    }
}

bool iambic_keyer_service(iambic_keyer_t *k, bool dit_contact, bool dah_contact, uint32_t now_ms)
{
    if (k->mode == IAMBIC_KEYER_MODE_STRAIGHT) {
        return dit_contact;
    }

    switch (k->state) {
    case IAMBIC_KEYER_STATE_IDLE:
        if (dit_contact) {
            start_element(k, true, now_ms);
        } else if (dah_contact) {
            start_element(k, false, now_ms);
        }
        break;

    case IAMBIC_KEYER_STATE_SEND_DIT:
    case IAMBIC_KEYER_STATE_SEND_DAH:
        latch_opposite(k, dit_contact, dah_contact);
        if (now_ms >= k->element_end_ms) {
            k->state = IAMBIC_KEYER_STATE_GAP;
            k->gap_end_ms = now_ms + k->unit_ms;
        }
        break;

    case IAMBIC_KEYER_STATE_GAP:
        latch_opposite(k, dit_contact, dah_contact);
        if (now_ms >= k->gap_end_ms) {
            bool opposite_now = k->sending_dit ? dah_contact : dit_contact;
            bool same_now = k->sending_dit ? dit_contact : dah_contact;

            if (opposite_now) {
                /* Squeeze alternation: the other paddle is (still, or newly) held
                 * right now, so continue alternating regardless of mode. */
                start_element(k, !k->sending_dit, now_ms);
                k->forced_extra = false;
            } else if (same_now) {
                /* Same paddle still held: repeat the same element. */
                start_element(k, k->sending_dit, now_ms);
                k->forced_extra = false;
            } else if (k->mode == IAMBIC_KEYER_MODE_B && k->opposite_latched && !k->forced_extra) {
                /* Mode B memory: the opposite paddle was tapped and released
                 * sometime during this element/gap. Send it once, then stop. */
                start_element(k, !k->sending_dit, now_ms);
                k->forced_extra = true;
            } else {
                k->state = IAMBIC_KEYER_STATE_IDLE;
                k->forced_extra = false;
            }
        }
        break;
    }

    return k->state == IAMBIC_KEYER_STATE_SEND_DIT || k->state == IAMBIC_KEYER_STATE_SEND_DAH;
}
