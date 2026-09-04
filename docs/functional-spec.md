# Morse Code Trainer — Functional Specification

## Purpose

A standalone Morse code **send trainer**: the operator keys Morse code on a
physical paddle (iambic) or straight key wired to the board, and the device
decodes the keying in real time into readable text shown on the touchscreen.
It does not teach receiving/copying code — it is a tool for practicing
*sending* clean, correctly-timed Morse.

## Hardware

- Board: ESP32-2432S028R ("Cheap Yellow Display"), ESP32, 2.8" 320×240
  ILI9341 SPI TFT, resistive XPT2046 touch controller, onboard speaker.
- External input: a two-lever iambic paddle (or a straight key), wired to
  two GPIOs on the board's header (dit and dah). A straight key is wired to
  the same GPIO used as "dit" in paddle mode.

## Screens and navigation

1. **Main Menu** — entry point on boot. Two buttons: "Practice" and
   "Settings".
2. **Practice screen** — reachable from the Main Menu.
   - Shows a scrolling text area of decoded characters, updated live as the
     operator keys.
   - Shows the currently active WPM and key mode (read-only, informational).
   - "Clear" button empties the decoded text and resets in-progress decode
     state.
   - "Back" button returns to the Main Menu.
   - Behavior: every completed character is appended to the text area;
     word gaps insert a space; a sequence that doesn't match any known
     Morse character is shown as a distinguishable placeholder (rather than
     silently dropped) so the operator can see a mis-keyed character
     happened.
3. **Settings screen** — reachable from the Main Menu.
   - **WPM** (words per minute, 5–40): controls dit/dah/gap timing.
   - **Key mode**: Iambic Mode A, Iambic Mode B, or Straight Key.
     - Iambic modes use both paddle GPIOs, alternating dit/dah while both
       are held ("squeezing"). Mode B additionally sends one extra
       alternate element if the opposite paddle was touched at all during
       the element just completed, even if released before its trailing
       gap ends — Mode A does not.
     - Straight Key mode ignores the dah GPIO; the dit GPIO is read
       directly as raw key-down/key-up with no automatic element timing —
       the operator controls dit/dah length by hand.
   - **Paddle swap**: swaps which physical paddle lever is treated as dit
     vs dah, for operators who prefer the opposite orientation.
   - **Sidetone frequency** (300–1200 Hz): pitch of the audible tone played
     while the key is down. A "Test tone" button plays the currently
     selected (not-yet-saved) frequency briefly so the operator can audition
     it.
   - "Save" button persists all four settings and applies them immediately
     to the running trainer (no reboot required).
   - "Back" button returns to the Main Menu.

## Behavior details

- **Sidetone**: an audible square-wave tone plays for the exact duration
  the key is logically down (post-debounce, post-decode-state), at the
  configured frequency — this applies identically whether driven by the
  iambic keyer's automatic element timing or by manual straight-key timing.
- **Debounce**: raw paddle/key contact closures are debounced in software
  so mechanical contact bounce does not corrupt timing or produce spurious
  short elements.
- **Persistence**: WPM, key mode, paddle swap, and sidetone frequency
  survive power loss. On boot, the last-saved values are loaded and applied
  before the operator can key anything.
- **Defaults** (first boot / no saved settings): 15 WPM, Iambic Mode B, no
  paddle swap, 600 Hz sidetone.

## Explicit non-goals (out of scope for this version)

- No receive/copy practice or quizzing (flashing/playing code for the user
  to identify).
- No Farnsworth spacing (character-speed vs. word-speed timing split).
- No statistics, history, or progress tracking.
- No Wi-Fi, OTA updates, or connectivity of any kind.
- No backlight dimming (backlight is simple on/off).
