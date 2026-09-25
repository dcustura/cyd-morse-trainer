#pragma once

#include <stdbool.h>
#include "morse_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Bring up a BLE HID keyboard peripheral (NimBLE transport) and start
 * advertising, so decoded Morse characters can be typed straight into a
 * paired phone or PC. A no-op if enabled is false: no radio, NimBLE host,
 * or GATT service is touched at all, so a user who never turns this on
 * doesn't pay its flash/RAM cost.
 *
 * Must be called once at boot with settings.ble_hid_enabled as it was
 * loaded from NVS; toggling the setting afterwards (Settings screen or the
 * CLI) only changes what gets loaded on the *next* boot, matching this
 * app's existing "Reset to Factory Defaults" restart-required convention,
 * rather than trying to bring the BT controller up/down live.
 */
void ble_hid_output_init(bool enabled);

/** True once a central (phone/PC) is connected and subscribed to reports. */
bool ble_hid_output_is_connected(void);

/**
 * Forward one paddle_input decode event as a keystroke. MORSE_CODEC_EVENT_CHAR
 * queues a key-down/key-up HID report for the decoded character, sent from
 * a dedicated task so this call never blocks (safe to call from
 * paddle_input's real-time keyer task). Characters with no sensible
 * keystroke -- MORSE_CODEC_EVENT_UNKNOWN and every decoded prosign (see
 * morse_codec.h) except HH, which sends Backspace -- are silently dropped.
 * MORSE_CODEC_EVENT_SPACE is forwarded only when the immediately preceding
 * thing actually sent was a real, non-Backspace keystroke -- never after a
 * dropped/unsendable char, and never right after HH's Backspace, since a
 * space in either of those spots isn't something the operator keyed, just
 * an artifact of the word-gap timing during the pause that followed. A
 * no-op if the feature wasn't enabled at boot or no central is currently
 * connected.
 */
void ble_hid_output_notify_char(morse_codec_event_t event, char decoded_char);

#ifdef __cplusplus
}
#endif
