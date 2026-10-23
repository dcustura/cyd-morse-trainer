#include "iambic_keyer.h"

/* Standard PARIS-timing gap lengths, in dit units, that
 * iambic_keyer_set_auto_space_pct()'s percentage is relative to. */
#define IAMBIC_KEYER_CHAR_GAP_STANDARD_UNITS 3u
#define IAMBIC_KEYER_WORD_GAP_STANDARD_UNITS 7u

/* The pre-Auto-Space behavior: never withhold. */
#define IAMBIC_KEYER_AUTO_SPACE_PCT_DISABLED 100u

static void reset_state(iambic_keyer_t *k)
{
    k->state = IAMBIC_KEYER_STATE_IDLE;
    k->element_end_ms = 0;
    k->gap_end_ms = 0;
    k->opposite_latched = false;
    k->forced_extra = false;
    k->sending_dit = false;
    k->ultimatic_last_dit = true; /* tie-break favors dit, matching the IDLE dit-first check */
    k->prev_dit_contact = false;
    k->prev_dah_contact = false;
    k->idle_has_reference = false;
    k->idle_start_ms = 0;
    k->auto_space_waiting = false;
    k->auto_space_wait_until_ms = 0;
    k->auto_space_dit_latched = false;
    k->auto_space_dah_latched = false;
}

void iambic_keyer_init(iambic_keyer_t *k, iambic_keyer_mode_t mode, uint16_t wpm)
{
    k->mode = mode;
    reset_state(k);
    iambic_keyer_set_wpm(k, wpm);
    iambic_keyer_set_auto_space_pct(k, IAMBIC_KEYER_AUTO_SPACE_PCT_DISABLED);
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

/* Round-half-up: standard_units scaled by pct%, as a whole number of dit
 * units (so the result stays in the same "N * unit_ms" shape the rest of
 * the keyer already schedules against, independent of the current WPM). */
static uint32_t round_half_up_units(uint32_t standard_units, uint8_t pct)
{
    return (standard_units * pct + 50u) / 100u;
}

void iambic_keyer_set_auto_space_pct(iambic_keyer_t *k, uint8_t pct)
{
    if (pct < 1u) {
        pct = 1u;
    } else if (pct > 100u) {
        pct = 100u;
    }

    uint32_t char_units = round_half_up_units(IAMBIC_KEYER_CHAR_GAP_STANDARD_UNITS, pct);
    uint32_t word_units = round_half_up_units(IAMBIC_KEYER_WORD_GAP_STANDARD_UNITS, pct);

    /* A char-gap threshold below 2 units would withhold on the normal
     * 1-unit gap between elements of the same character. */
    if (char_units < 2u) {
        char_units = 2u;
    }
    /* Keep the word-gap threshold strictly past the char-gap one, and past
     * the char standard, so a just-completed character gap never also
     * triggers a word-gap withhold. */
    if (word_units <= IAMBIC_KEYER_CHAR_GAP_STANDARD_UNITS) {
        word_units = IAMBIC_KEYER_CHAR_GAP_STANDARD_UNITS + 1u;
    }

    k->auto_space_char_threshold_units = (uint8_t)char_units;
    k->auto_space_word_threshold_units = (uint8_t)word_units;
}

/*
 * The real-silence target (in ms, since the key-up edge that ended the
 * last character) the keyer should withhold a newly-touched paddle up to,
 * given idle_elapsed_ms has already passed since that edge. Returns 0 if
 * no withholding is needed -- either the pause is still below the
 * configured threshold (too tight to call intentional), or it has already
 * reached the standard gap on its own.
 */
static uint32_t auto_space_pad_target_ms(const iambic_keyer_t *k, uint32_t idle_elapsed_ms)
{
    uint32_t char_threshold_ms = (uint32_t)k->auto_space_char_threshold_units * k->unit_ms;
    uint32_t char_standard_ms = IAMBIC_KEYER_CHAR_GAP_STANDARD_UNITS * k->unit_ms;
    uint32_t word_threshold_ms = (uint32_t)k->auto_space_word_threshold_units * k->unit_ms;
    uint32_t word_standard_ms = IAMBIC_KEYER_WORD_GAP_STANDARD_UNITS * k->unit_ms;

    if (idle_elapsed_ms >= char_threshold_ms && idle_elapsed_ms < char_standard_ms) {
        return char_standard_ms;
    }
    if (idle_elapsed_ms >= word_threshold_ms && idle_elapsed_ms < word_standard_ms) {
        return word_standard_ms;
    }
    return 0u;
}

/*
 * Shared by service_iambic()/service_ultimatic()'s IDLE case: decides
 * whether a touched paddle should start immediately or be withheld (and
 * latched) for Auto Space. Returns true once it's time to actually start
 * an element, with want_dit/want_dah (out params) set to what should
 * start (which may differ from the current dit_contact/dah_contact if
 * they came from an earlier, now-released touch latched during the
 * withhold). Returns false if the caller should do nothing more this tick
 * (either nothing was touched, or Auto Space is still withholding).
 */
static bool auto_space_resolve(iambic_keyer_t *k, bool dit_contact, bool dah_contact, uint32_t now_ms,
                                bool *want_dit, bool *want_dah)
{
    *want_dit = dit_contact;
    *want_dah = dah_contact;

    if (k->auto_space_waiting) {
        k->auto_space_dit_latched = k->auto_space_dit_latched || dit_contact;
        k->auto_space_dah_latched = k->auto_space_dah_latched || dah_contact;
        if (now_ms < k->auto_space_wait_until_ms) {
            return false;
        }
        *want_dit = k->auto_space_dit_latched;
        *want_dah = k->auto_space_dah_latched;
        k->auto_space_waiting = false;
        k->auto_space_dit_latched = false;
        k->auto_space_dah_latched = false;
        return true;
    }

    if (!dit_contact && !dah_contact) {
        return false;
    }

    if (k->idle_has_reference) {
        uint32_t idle_elapsed_ms = now_ms - k->idle_start_ms;
        uint32_t target_ms = auto_space_pad_target_ms(k, idle_elapsed_ms);
        if (target_ms > 0u) {
            k->auto_space_waiting = true;
            k->auto_space_wait_until_ms = k->idle_start_ms + target_ms;
            k->auto_space_dit_latched = dit_contact;
            k->auto_space_dah_latched = dah_contact;
            return false;
        }
    }

    return true;
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

static bool is_sending(const iambic_keyer_t *k)
{
    return k->state == IAMBIC_KEYER_STATE_SEND_DIT || k->state == IAMBIC_KEYER_STATE_SEND_DAH;
}

/* Shared by service_iambic()/service_ultimatic()'s GAP case, on the branch
 * where nothing more is queued and the keyer is really going idle: records
 * the Auto Space reference point. element_end_ms (not gap_end_ms -
 * unit_ms) is used deliberately -- it was set once, in start_element(),
 * and never touched again, so it still holds the exact key-up edge that
 * ended the last element even if the WPM (and so unit_ms) changed at some
 * point during the trailing gap that followed it. */
static void enter_idle(iambic_keyer_t *k)
{
    k->state = IAMBIC_KEYER_STATE_IDLE;
    k->idle_start_ms = k->element_end_ms;
    k->idle_has_reference = true;
}

/* Shared SEND_DIT/SEND_DAH handling: latch a squeeze-tap on the opposite
 * paddle, then move on to GAP once the element's duration has elapsed. Same
 * in Iambic and Ultimatic. */
static void advance_sending(iambic_keyer_t *k, bool dit_contact, bool dah_contact, uint32_t now_ms)
{
    latch_opposite(k, dit_contact, dah_contact);
    if (now_ms >= k->element_end_ms) {
        k->state = IAMBIC_KEYER_STATE_GAP;
        k->gap_end_ms = now_ms + k->unit_ms;
    }
}

static bool service_iambic(iambic_keyer_t *k, bool dit_contact, bool dah_contact, uint32_t now_ms)
{
    switch (k->state) {
    case IAMBIC_KEYER_STATE_IDLE: {
        bool want_dit, want_dah;
        if (auto_space_resolve(k, dit_contact, dah_contact, now_ms, &want_dit, &want_dah)) {
            if (want_dit) {
                start_element(k, true, now_ms);
            } else if (want_dah) {
                start_element(k, false, now_ms);
            }
        }
        break;
    }

    case IAMBIC_KEYER_STATE_SEND_DIT:
    case IAMBIC_KEYER_STATE_SEND_DAH:
        advance_sending(k, dit_contact, dah_contact, now_ms);
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
            } else if (same_now && k->opposite_latched && !k->forced_extra) {
                /* The held paddle is still down, but the opposite paddle was
                 * tapped and released sometime during this element/gap:
                 * insert it now instead of just repeating the held paddle.
                 * This is plain squeeze keying and applies in both modes --
                 * it is not the mode-B "full release" memory below. */
                start_element(k, !k->sending_dit, now_ms);
                k->forced_extra = true;
            } else if (same_now) {
                /* Same paddle still held, nothing to insert: repeat the same element. */
                start_element(k, k->sending_dit, now_ms);
                k->forced_extra = false;
            } else if (k->mode == IAMBIC_KEYER_MODE_B && k->opposite_latched && !k->forced_extra) {
                /* Mode B memory: the opposite paddle was tapped and released
                 * sometime during this element/gap. Send it once, then stop. */
                start_element(k, !k->sending_dit, now_ms);
                k->forced_extra = true;
            } else {
                enter_idle(k);
                k->forced_extra = false;
            }
        }
        break;
    }

    return is_sending(k);
}

static bool service_ultimatic(iambic_keyer_t *k, bool dit_contact, bool dah_contact, uint32_t now_ms)
{
    /* Track which paddle most recently transitioned pressed, to resolve
     * squeeze priority. dah is checked first, dit second, so that a
     * same-tick double rising-edge ties toward dit, consistent with the
     * dit-first checks below and in service_iambic's IDLE case. */
    if (dah_contact && !k->prev_dah_contact) {
        k->ultimatic_last_dit = false;
    }
    if (dit_contact && !k->prev_dit_contact) {
        k->ultimatic_last_dit = true;
    }
    k->prev_dit_contact = dit_contact;
    k->prev_dah_contact = dah_contact;

    switch (k->state) {
    case IAMBIC_KEYER_STATE_IDLE: {
        bool want_dit, want_dah;
        if (auto_space_resolve(k, dit_contact, dah_contact, now_ms, &want_dit, &want_dah)) {
            if (want_dit && want_dah) {
                start_element(k, k->ultimatic_last_dit, now_ms);
            } else if (want_dit) {
                start_element(k, true, now_ms);
            } else if (want_dah) {
                start_element(k, false, now_ms);
            }
        }
        break;
    }

    case IAMBIC_KEYER_STATE_SEND_DIT:
    case IAMBIC_KEYER_STATE_SEND_DAH:
        advance_sending(k, dit_contact, dah_contact, now_ms);
        break;

    case IAMBIC_KEYER_STATE_GAP:
        latch_opposite(k, dit_contact, dah_contact);
        if (now_ms >= k->gap_end_ms) {
            bool same_now = k->sending_dit ? dit_contact : dah_contact;

            if (dit_contact && dah_contact) {
                /* Both paddles are actually down right now: a live squeeze,
                 * decided purely by whichever was pressed more recently --
                 * no alternation, unlike Iambic. */
                start_element(k, k->ultimatic_last_dit, now_ms);
            } else if (same_now && k->opposite_latched) {
                /* The held paddle is still down, but the opposite paddle
                 * was tapped and released sometime during this
                 * element/gap: insert it once, then fall back to whichever
                 * paddle is actually held on the next decision -- this is
                 * plain squeeze-insert, the same as Iambic's, not a
                 * standing alternation and not memory across a full
                 * release (see the idle fallback below). */
                start_element(k, !k->sending_dit, now_ms);
            } else if (dit_contact) {
                start_element(k, true, now_ms);
            } else if (dah_contact) {
                start_element(k, false, now_ms);
            } else {
                enter_idle(k);
            }
        }
        break;
    }

    return is_sending(k);
}

bool iambic_keyer_service(iambic_keyer_t *k, bool dit_contact, bool dah_contact, uint32_t now_ms)
{
    switch (k->mode) {
    case IAMBIC_KEYER_MODE_STRAIGHT:
        return dit_contact;
    case IAMBIC_KEYER_MODE_ULTIMATIC:
        return service_ultimatic(k, dit_contact, dah_contact, now_ms);
    default: /* IAMBIC_KEYER_MODE_A, IAMBIC_KEYER_MODE_B */
        return service_iambic(k, dit_contact, dah_contact, now_ms);
    }
}
