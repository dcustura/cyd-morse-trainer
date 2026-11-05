#include "message_player.h"

#include <string.h>

#include "iambic_keyer.h"
#include "morse_codec.h"

#define DAH_UNITS 3u
#define CHAR_GAP_UNITS 3u
#define WORD_GAP_UNITS 7u

static bool time_reached(uint32_t now_ms, uint32_t t_ms)
{
    return (int32_t)(now_ms - t_ms) >= 0;
}

static uint8_t clamp_u8(uint8_t v, uint8_t lo, uint8_t hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

static void set_phase(message_player_t *p, message_player_phase_t phase, uint32_t start_ms, uint32_t duration_ms)
{
    p->phase = phase;
    p->phase_start_ms = start_ms;
    p->phase_end_ms = start_ms + duration_ms;
}

static void go_idle(message_player_t *p)
{
    p->phase = MESSAGE_PLAYER_PHASE_IDLE;
    p->queue_len = 0;
}

static bool more_repeats(const message_player_t *p)
{
    return p->current.repeat && (p->current.count == 0 || p->sends_done < p->current.count);
}

static int32_t weight_extra_ms(const message_player_t *p)
{
    return iambic_keyer_weight_extra_ms(p->unit_ms, p->weight_pct);
}

/* A gap that follows a key-up: weight takes from (or adds to) it whatever
 * it added to (or took from) the element before -- that element's own
 * extra, so element + gap stays right even if weight changed meanwhile. */
static uint32_t gap_after_element_ms(const message_player_t *p, uint32_t units)
{
    return (uint32_t)((int32_t)(units * p->unit_ms) - p->element_extra_ms);
}

static void key_element(message_player_t *p, uint32_t t_ms)
{
    uint32_t units = (p->token.pattern[p->element] == '-') ? DAH_UNITS : 1u;
    p->element_extra_ms = weight_extra_ms(p);
    uint32_t ms = (uint32_t)((int32_t)(units * p->unit_ms) + p->element_extra_ms);
    set_phase(p, MESSAGE_PLAYER_PHASE_KEY_DOWN, t_ms, ms);
}

static void start_transmission(message_player_t *p, uint32_t t_ms)
{
    p->cursor = 0;
    p->element = 0;
    /* Text was checked to have a token when it was enqueued. */
    morse_text_next_token(p->current.text, &p->cursor, &p->token);
    key_element(p, t_ms);
}

/* Make the first queued message current, starting a word gap after
 * gap_start_ms (but no earlier than now_ms, so it never starts in the past). */
static void start_next_queued(message_player_t *p, uint32_t gap_start_ms, uint32_t now_ms)
{
    p->current = p->queue[0];
    p->queue_len--;
    memmove(&p->queue[0], &p->queue[1], p->queue_len * sizeof(p->queue[0]));
    p->sends_done = 0;
    p->lead_in_is_pause = false;
    uint32_t end_ms = gap_start_ms + gap_after_element_ms(p, WORD_GAP_UNITS);
    if (time_reached(now_ms, end_ms)) {
        end_ms = now_ms;
    }
    set_phase(p, MESSAGE_PLAYER_PHASE_LEAD_IN, gap_start_ms, end_ms - gap_start_ms);
}

static void end_transmission(message_player_t *p, uint32_t t_ms)
{
    p->sends_done++;
    if (p->queue_len > 0) {
        start_next_queued(p, t_ms, t_ms);
    } else if (more_repeats(p)) {
        p->lead_in_is_pause = true;
        set_phase(p, MESSAGE_PLAYER_PHASE_LEAD_IN, t_ms, (uint32_t)p->current.pause_s * 1000u);
    } else {
        go_idle(p);
    }
}

/* The current phase ended at t_ms; move to the next one. */
static void advance(message_player_t *p, uint32_t t_ms)
{
    switch (p->phase) {
    case MESSAGE_PLAYER_PHASE_LEAD_IN:
        start_transmission(p, t_ms);
        break;

    case MESSAGE_PLAYER_PHASE_KEY_DOWN: {
        p->element++;
        if (p->token.pattern[p->element] != '\0') {
            set_phase(p, MESSAGE_PLAYER_PHASE_KEY_UP, t_ms, gap_after_element_ms(p, 1u));
            break;
        }
        size_t peek = p->cursor;
        morse_text_token_t next;
        if (!morse_text_next_token(p->current.text, &peek, &next)) {
            end_transmission(p, t_ms);
            break;
        }
        uint32_t units = next.word_gap_before ? WORD_GAP_UNITS : CHAR_GAP_UNITS;
        set_phase(p, MESSAGE_PLAYER_PHASE_KEY_UP, t_ms, gap_after_element_ms(p, units));
        break;
    }

    case MESSAGE_PLAYER_PHASE_KEY_UP:
        if (p->token.pattern[p->element] == '\0') {
            /* A character or word gap ended: move on to the next token. */
            morse_text_next_token(p->current.text, &p->cursor, &p->token);
            p->element = 0;
        }
        key_element(p, t_ms);
        break;

    case MESSAGE_PLAYER_PHASE_IDLE:
        break;
    }
}

void message_player_init(message_player_t *p, uint16_t wpm)
{
    memset(p, 0, sizeof(*p));
    p->phase = MESSAGE_PLAYER_PHASE_IDLE;
    message_player_set_wpm(p, wpm);
    message_player_set_weight_pct(p, IAMBIC_KEYER_WEIGHT_PCT_STANDARD);
}

void message_player_set_wpm(message_player_t *p, uint16_t wpm)
{
    p->unit_ms = morse_codec_wpm_to_unit_ms(wpm);
}

void message_player_set_weight_pct(message_player_t *p, uint8_t pct)
{
    p->weight_pct = iambic_keyer_clamp_weight_pct(pct);
}

bool message_player_tag_pending(const message_player_t *p, int tag, message_player_enqueue_result_t *why)
{
    if (p->phase == MESSAGE_PLAYER_PHASE_IDLE) {
        return false; /* p->current is stale once idle */
    }
    if (tag == p->current.tag) {
        *why = MESSAGE_PLAYER_IGNORED_PLAYING;
        return true;
    }
    for (uint8_t i = 0; i < p->queue_len; ++i) {
        if (p->queue[i].tag == tag) {
            *why = MESSAGE_PLAYER_IGNORED_QUEUED;
            return true;
        }
    }
    return false;
}

message_player_enqueue_result_t message_player_enqueue(message_player_t *p, const message_player_message_t *msg,
                                                       uint32_t now_ms)
{
    /* Terminate first: the caller's text may fill the whole array. */
    message_player_message_t copy = *msg;
    copy.text[MESSAGE_PLAYER_TEXT_MAX] = '\0';

    size_t pos = 0;
    morse_text_token_t first;
    if (!morse_text_next_token(copy.text, &pos, &first)) {
        return MESSAGE_PLAYER_INVALID;
    }

    copy.count = clamp_u8(copy.count, 0, MESSAGE_PLAYER_COUNT_MAX);
    copy.pause_s = clamp_u8(copy.pause_s, MESSAGE_PLAYER_PAUSE_MIN_S, MESSAGE_PLAYER_PAUSE_MAX_S);

    if (p->phase == MESSAGE_PLAYER_PHASE_IDLE) {
        p->current = copy;
        p->sends_done = 0;
        p->lead_in_is_pause = false;
        /* The operator's keyer is weighted the same way, so the silence
         * after their last element is a weighted word gap, as between
         * queued messages. */
        uint32_t word_gap_ms = (uint32_t)((int32_t)(WORD_GAP_UNITS * p->unit_ms) - weight_extra_ms(p));
        uint32_t since_key_ms = now_ms - p->key_down_ms;
        if (p->key_down_seen && since_key_ms < word_gap_ms) {
            /* The operator keyed moments ago: their element is cut off here
             * (the message now owns the key), then a word gap. */
            set_phase(p, MESSAGE_PLAYER_PHASE_LEAD_IN, now_ms, word_gap_ms - since_key_ms);
        } else {
            start_transmission(p, now_ms);
        }
        return MESSAGE_PLAYER_STARTED;
    }
    message_player_enqueue_result_t why;
    if (message_player_tag_pending(p, copy.tag, &why)) {
        return why;
    }
    if (p->queue_len >= MESSAGE_PLAYER_QUEUE_MAX) {
        return MESSAGE_PLAYER_QUEUE_FULL;
    }
    p->queue[p->queue_len++] = copy;

    if (p->phase == MESSAGE_PLAYER_PHASE_LEAD_IN && p->lead_in_is_pause) {
        /* The repeating message's last transmission already ended. */
        start_next_queued(p, p->phase_start_ms, now_ms);
        return MESSAGE_PLAYER_STARTED;
    }
    return MESSAGE_PLAYER_QUEUED;
}

void message_player_note_key(message_player_t *p, bool key_down, uint32_t now_ms)
{
    if (key_down) {
        p->key_down_seen = true;
        p->key_down_ms = now_ms;
    } else if (p->key_down_seen && now_ms - p->key_down_ms >= WORD_GAP_UNITS * p->unit_ms) {
        p->key_down_seen = false;
    }
}

void message_player_stop(message_player_t *p)
{
    go_idle(p);
}

static bool in_pause(const message_player_t *p)
{
    return p->phase == MESSAGE_PLAYER_PHASE_LEAD_IN && p->lead_in_is_pause;
}

void message_player_set_repeat(message_player_t *p, bool repeat)
{
    if (p->phase == MESSAGE_PLAYER_PHASE_IDLE) {
        return;
    }
    p->current.repeat = repeat;
    if (in_pause(p) && !more_repeats(p)) {
        go_idle(p);
    }
}

void message_player_set_repeat_params(message_player_t *p, uint8_t count, uint8_t pause_s, uint32_t now_ms)
{
    if (p->phase == MESSAGE_PLAYER_PHASE_IDLE) {
        return;
    }
    p->current.count = clamp_u8(count, 0, MESSAGE_PLAYER_COUNT_MAX);
    p->current.pause_s = clamp_u8(pause_s, MESSAGE_PLAYER_PAUSE_MIN_S, MESSAGE_PLAYER_PAUSE_MAX_S);
    if (in_pause(p)) {
        if (!more_repeats(p)) {
            go_idle(p);
        } else {
            p->phase_end_ms = p->phase_start_ms + (uint32_t)p->current.pause_s * 1000u;
            /* A shorter pause that has already run out ends now, not in the
             * past: service() would otherwise catch up through a whole
             * transmission without keying it. */
            if (time_reached(now_ms, p->phase_end_ms)) {
                p->phase_end_ms = now_ms;
            }
        }
    }
}

bool message_player_service(message_player_t *p, uint32_t now_ms)
{
    /* Each phase starts exactly when the previous one ended, so timing
     * doesn't drift with the caller's polling jitter. */
    while (p->phase != MESSAGE_PLAYER_PHASE_IDLE && time_reached(now_ms, p->phase_end_ms)) {
        advance(p, p->phase_end_ms);
    }
    return p->phase == MESSAGE_PLAYER_PHASE_KEY_DOWN;
}

bool message_player_is_active(const message_player_t *p)
{
    return p->phase != MESSAGE_PLAYER_PHASE_IDLE;
}

void message_player_get_status(const message_player_t *p, uint32_t now_ms, message_player_status_t *out)
{
    memset(out, 0, sizeof(*out));
    out->active = message_player_is_active(p);
    if (!out->active) {
        return;
    }
    out->paused = in_pause(p);
    out->tag = p->current.tag;
    out->text = p->current.text;
    if (p->phase != MESSAGE_PLAYER_PHASE_LEAD_IN) {
        out->char_pos = p->token.text_pos;
        out->char_len = p->token.text_len;
    }
    out->send_number = p->sends_done + 1;
    out->repeat = p->current.repeat;
    out->count = p->current.count;
    out->pause_s = p->current.pause_s;
    if (out->paused && !time_reached(now_ms, p->phase_end_ms)) {
        out->pause_remaining_ms = p->phase_end_ms - now_ms;
    }
    out->queue_len = p->queue_len;
    for (uint8_t i = 0; i < p->queue_len; ++i) {
        out->queue_tags[i] = p->queue[i].tag;
    }
}
