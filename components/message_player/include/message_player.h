#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "morse_text.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Message memory playback: sends normalized message text (see morse_text.h)
 * as a key-down/key-up sequence, with repeat count and pause, and a queue of
 * messages to send next.
 *
 * Pure logic, like iambic_keyer: callers pass the current time to every
 * call, and message_player_service() returns the key state for that time.
 * Not thread-safe; callers that use it from more than one task must
 * serialize access.
 *
 * Timing is standard PARIS at the current WPM: dit 1 unit, dah 3, element
 * gap 1, character gap 3, word gap 7. Each element or gap's length is fixed
 * when it starts, so a WPM change applies from the next one.
 */

#define MESSAGE_PLAYER_TEXT_MAX 80 /* chars, excluding the terminator */
#define MESSAGE_PLAYER_QUEUE_MAX 6 /* messages waiting behind the current one */
#define MESSAGE_PLAYER_PAUSE_MIN_S 1
#define MESSAGE_PLAYER_PAUSE_MAX_S 99
#define MESSAGE_PLAYER_COUNT_MAX 99

typedef struct {
    char text[MESSAGE_PLAYER_TEXT_MAX + 1]; /* normalized text */
    bool repeat;
    uint8_t count;   /* total transmissions when repeating; 0 = until stopped */
    uint8_t pause_s; /* silence between repeated transmissions */
    int tag;         /* caller's id for the message (e.g. its tile) */
} message_player_message_t;

typedef enum {
    MESSAGE_PLAYER_STARTED,         /* now the current message */
    MESSAGE_PLAYER_QUEUED,          /* waiting behind the current one */
    MESSAGE_PLAYER_IGNORED_PLAYING, /* its tag is the current message's */
    MESSAGE_PLAYER_IGNORED_QUEUED,  /* its tag is already in the queue */
    MESSAGE_PLAYER_QUEUE_FULL,
    MESSAGE_PLAYER_INVALID,         /* no sendable text */
} message_player_enqueue_result_t;

typedef enum {
    MESSAGE_PLAYER_PHASE_IDLE,
    MESSAGE_PLAYER_PHASE_LEAD_IN, /* silence before a transmission */
    MESSAGE_PLAYER_PHASE_KEY_DOWN,
    MESSAGE_PLAYER_PHASE_KEY_UP,  /* element, character or word gap */
} message_player_phase_t;

typedef struct {
    message_player_message_t current;
    message_player_message_t queue[MESSAGE_PLAYER_QUEUE_MAX];
    uint8_t queue_len;

    message_player_phase_t phase;
    bool lead_in_is_pause; /* lead-in is a repeat pause, not a word gap */
    uint32_t phase_start_ms;
    uint32_t phase_end_ms;
    uint32_t unit_ms;
    uint8_t weight_pct; /* see message_player_set_weight_pct() */
    int32_t element_extra_ms; /* weight extra of the last element keyed; the gap after it gives it back */
    uint32_t sends_done; /* finished transmissions of the current message */

    size_t cursor;             /* text offset after the token being sent */
    morse_text_token_t token;  /* token being sent */
    uint8_t element;           /* index into token.pattern */

    bool key_down_seen;    /* key_down_ms is valid */
    uint32_t key_down_ms;  /* last time the operator's own key was down */
} message_player_t;

typedef struct {
    bool active;           /* sending, pausing, or in a gap before a queued message */
    bool paused;           /* in a repeat pause */
    int tag;               /* current message's tag; valid while active */
    const char *text;      /* current message's text; valid while active */
    size_t char_pos;       /* character being sent, for highlighting... */
    size_t char_len;       /* ...0 when none (e.g. during a lead-in) */
    uint32_t send_number;  /* 1-based: the transmission in progress, or the next one during a pause */
    bool repeat;
    uint8_t count;         /* 0 = until stopped */
    uint8_t pause_s;
    uint32_t pause_remaining_ms; /* during a pause */
    uint8_t queue_len;
    int queue_tags[MESSAGE_PLAYER_QUEUE_MAX];
} message_player_status_t;

void message_player_init(message_player_t *p, uint16_t wpm);

/** Change speed; applies from the next element or gap. */
void message_player_set_wpm(message_player_t *p, uint16_t wpm);

/**
 * Weight, the same as the keyer's (see iambic_keyer_set_weight_pct()):
 * each element gains (or loses) iambic_keyer_weight_extra_ms() and the gap
 * after it loses (or gains) the same, so speed is unchanged. Clamped to
 * IAMBIC_KEYER_WEIGHT_PCT_MIN..MAX; 50 (standard) after init. Applies from
 * the next element or gap.
 */
void message_player_set_weight_pct(message_player_t *p, uint8_t pct);

/**
 * Send msg now if idle, else queue it. If idle but the operator's own key
 * was down less than a word gap ago (see message_player_note_key()),
 * the message starts a word gap after that instead, so its first element
 * never joins or runs into the operator's last one. While the current message is
 * repeating, queueing makes it end after its current transmission (or at
 * once, during a pause). A queued message starts a word gap after the end
 * of the previous transmission. msg->count and msg->pause_s are clamped.
 */
message_player_enqueue_result_t message_player_enqueue(message_player_t *p, const message_player_message_t *msg,
                                                       uint32_t now_ms);

/**
 * The operator's own key (not this player's) state at now_ms. Call every
 * tick while idle: a key-down a word gap old is forgotten, so it can't
 * look recent again once the millisecond counter wraps (~49.7 days).
 */
void message_player_note_key(message_player_t *p, bool key_down, uint32_t now_ms);

/** Stop at once: key up, current message and queue dropped. */
void message_player_stop(message_player_t *p);

/**
 * Turn repeat on/off for the current message. Turning it off during a
 * transmission lets it finish, then stops; during a pause, stops at once.
 * No-op while idle.
 */
void message_player_set_repeat(message_player_t *p, bool repeat);

/**
 * Change the current message's repeat count and pause (clamped). During a
 * pause, the new pause counts from the pause's start (ending at now_ms if
 * that time has already passed), and a count already reached stops at once.
 * No-op while idle.
 */
void message_player_set_repeat_params(message_player_t *p, uint8_t count, uint8_t pause_s, uint32_t now_ms);

/** Advance to now_ms and return whether the key is down. Call every ~1 ms. */
bool message_player_service(message_player_t *p, uint32_t now_ms);

bool message_player_is_active(const message_player_t *p);

void message_player_get_status(const message_player_t *p, uint32_t now_ms, message_player_status_t *out);

#ifdef __cplusplus
}
#endif
