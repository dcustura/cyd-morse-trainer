# Morse Code Trainer — Functional Specification

## Purpose

A standalone Morse code **send trainer**: the operator keys Morse code on a
physical paddle (iambic) or straight key wired to the board, and the device
decodes the keying in real time into readable text shown on the touchscreen.
It does not teach receiving/copying code — it is a tool for practicing
*sending* clean, correctly-timed Morse.

## Hardware

- Board: ESP32-2432S028R ("Cheap Yellow Display"), ESP32, 2.8" 320×240
  ILI9341 SPI TFT, resistive XPT2046 touch controller, onboard speaker, and
  an onboard LDR (ambient light sensor) used for auto-brightness (see the
  Brightness setting below; non-functional on stock boards — see the
  README's Hardware section).
- External input: a two-lever iambic paddle (or a straight key), wired to
  two GPIOs on the board's header (dit and dah). A straight key is wired to
  the same GPIO used as "dit" in paddle mode.

## Boot flow

- If no touch calibration has been saved yet (first boot, or after a
  factory reset), the device boots directly into the **Touch Calibration**
  screen instead of the Main Menu, since touch input isn't usable yet.
- On every other boot, the last-saved touch calibration and keyer settings
  are loaded and applied automatically, and the device starts at the
  **Main Menu**.

## Screens and navigation

1. **Main Menu** — entry point on boot (once calibrated). Two buttons:
   "Practice" and "Settings".
2. **Practice screen** — reachable from the Main Menu.
   - Shows a scrolling text area of decoded characters, updated live as the
     operator keys, in one of two sizes set by the Text Size setting
     (Settings' Display submenu): Small (the compact bitmap font, ~26
     characters per visible line at the default width) or Large (a smoother,
     anti-aliased custom monospace font sized for easier reading at a
     distance, fewer characters per line). Changing the setting restyles the
     text area immediately without losing its content.
   - Shows the currently active WPM and key mode (read-only, informational),
     refreshed whenever Settings are saved.
   - A small red indicator dot (bottom-right of the screen, vertically
     centered on the Clear/Back buttons) lights up whenever the key/paddle
     is down and the sidetone is sounding, and goes dark when it releases.
   - "Clear" button empties the decoded text and resets in-progress decode
     state.
   - "Back" button returns to the Main Menu.
   - Behavior: every completed character is appended to the text area;
     word gaps insert a space.
   - Decodable characters: the 26 letters, 10 digits, and standard Morse
     punctuation (`. , ? ' ! / ( ) & : ; = + - _ " $ @`).
   - Prosigns (procedural signals, sent as one unbroken sequence with no
     gap between "letters") are recognized too. Ones whose timing matches
     an existing punctuation mark decode to that punctuation glyph, by
     convention: BT → `=`, AR → `+`, KN → `(`, AS → `&`. Sending BT or AR
     also forces a line break right after it, since both conventionally end
     a line of text. The remaining prosigns with no punctuation
     equivalent — SK, HH, VE, CT — are shown as green `/XX` text (e.g.
     `/SK`), so they read as a single procedural unit rather than two
     ordinary letters, with two exceptions that act as control characters
     instead of being displayed:
     - **HH** ("error, keyed over") erases one unit at a time instead of
       appearing in the text, matching its traditional meaning as a
       correction signal: each HH trims the last (partial or complete) word,
       or deletes a mis-keyed `*` placeholder outright if that's what's
       currently last. Sending it repeatedly walks back indefinitely through
       however many words and `*` placeholders were sent since the last
       forced line break or prosign badge, like repeated word-backspace,
       regardless of how much real text separates them from the current
       position. HH never reaches back past a forced line break or a real
       prosign badge; right after one, with nothing typed since, it's a
       no-op.
     - **SK** ("end of contact") displays its `/SK` badge as usual, then
       forces a blank line afterward, so the next transmission starts
       clearly separated.
   - A sequence that doesn't match any known character or prosign is shown
     as a distinguishable placeholder (`*`, in red) rather than silently
     dropped, so the operator can see a mis-keyed character happened.
3. **Settings screen** — reachable from the Main Menu.
   - A grid of large, tappable tiles (three columns × two rows) filling the
     whole display with no scrolling and no separate title bar: Keyer,
     Sidetone, and Display fill the top row; the bottom row has Reset to
     Defaults and a Back tile (marked with a left-chevron icon), leaving one
     cell empty. There is no slider or dropdown anywhere on this screen or
     its submenus, and no OK/Cancel step — every change is applied to the
     running trainer and persisted to NVS the moment it's made (writes are
     briefly debounced so rapid taps or a held `-`/`+` button don't hit NVS
     on every step). Back returns to the Main Menu.
   - Every submenu screen (Keyer/Sidetone/Display/Touchscreen) follows the
     same pattern as this top-level grid: no separate title bar, and Back
     (left-chevron icon) is just the last tile in the grid, sized N×M with
     N = M or N = M + 1 so tiles stay wide rather than tall and narrow.
   - **Keyer** tile opens a submenu screen with a 3×2 grid of tiles (one
     cell left empty), each opening a popup to change just that value:
     - **WPM** (words per minute, 5–40): controls dit/dah/gap timing. Its
       popup has `-`/`+` buttons (press-and-hold to repeat).
     - **Key mode**: Iambic Mode A, Iambic Mode B, Straight Key, or
       Ultimatic. Its popup lists all four as a 3-column × 2-row grid of
       finger-sized buttons ("Mode A"/"Mode B"/"Straight"/"Ultimatic",
       with "Close" filling the last cell); tapping one selects it
       immediately.
       - Iambic modes use both paddle GPIOs, alternating dit/dah while
         both are held ("squeezing"). Briefly tapping the opposite paddle
         while the primary paddle is still held (e.g. dit-dah-dit)
         latches that tap and inserts it at the next element boundary
         even though the primary paddle was never released — standard
         squeeze-keying behavior, identical in Mode A and Mode B.
       - Mode B additionally sends one extra alternate element if the
         opposite paddle was touched at all during the element just
         completed, even if released before its trailing gap ends — Mode
         A does not. This full-release memory is separate from, and in
         addition to, the squeeze-tap latching above.
       - Straight Key mode ignores the dah GPIO; the dit GPIO is read
         directly as raw key-down/key-up with no automatic element
         timing — the operator controls dit/dah length by hand.
       - Ultimatic mode also uses both paddle GPIOs, but doesn't
         alternate on a live squeeze: whichever paddle was pressed more
         recently keeps repeating for as long as both are held (a
         same-instant first press ties toward dit). Briefly tapping the
         opposite paddle while the primary is still held still latches
         and inserts that tap once, the same squeeze-insert behavior as
         Iambic — but unlike Mode B, nothing is remembered once both
         paddles are released; there is no full-release memory.
     - **Paddle swap**: swaps which physical paddle lever is treated as
       dit vs dah, for operators who prefer the opposite orientation. Its
       popup offers "Normal"/"Swapped" buttons; tapping one selects it
       immediately.
     - **Paddle debounce**: enables/disables software debounce while in
       Iambic Mode A/B/Ultimatic. Its popup offers "On"/"Off" buttons;
       tapping one selects it immediately. This setting has no effect in
       Straight Key mode, which is always debounced (a mechanical
       straight key bounces; a clean paddle contact often does not need
       the extra latency).
   - **Sidetone** tile opens a submenu screen with a 2×2 grid of tiles (one
     cell left empty), each opening a popup to change just that value:
     - **Pitch** (300–1200 Hz): pitch of the audible sidetone played while
       the key is down. Its popup has `-`/`+` buttons and a "Test" button
       that briefly plays the sidetone at its current settings.
     - **Volume** (0–100%): loudness of the sidetone as a percentage of
       full amplitude. Its popup has `-`/`+` buttons and the same "Test"
       button.
     - **Smoothing** (2–100 ms): duration of the attack/decay ramp shaping
       each key-down/key-up transition, to avoid audible keying clicks.
       Its popup has `-`/`+` buttons and the same "Test" button.
   - **Display** tile opens a submenu screen with a 2×2 grid of tiles:
     - **Brightness** (10–100%): backlight brightness. Its popup has `-`/`+`
       buttons and an "Auto"/"Manual" toggle button; toggling to Auto drives
       brightness from the onboard LDR (ambient light sensor) instead, and
       stepping `-`/`+` always switches back to Manual. On stock
       ESP32-2432S028R boards, Auto has no visible effect (the LDR divider
       reads a flat, unlit value regardless of ambient light) — see the
       README's Hardware section.
     - **Touchscreen** tile opens a further submenu screen with a 2×2 grid
       of tiles (one cell left empty):
       - "Calibrate" opens the Touch Calibration screen to redo calibration
         (its Back button returns to this submenu).
       - "Verify Calibration" opens the Verify Calibration screen to check
         the current calibration visually (its swipe-Back also returns to
         this submenu).
     - **Text Size**: Small or Large decoded-text font on the Practice
       screen (see the Practice screen's description above). Its popup
       offers "Small"/"Large" buttons; tapping one selects it immediately.
   - "Reset to Defaults" tile, after a Yes/No confirmation, erases both
     the keyer settings and the touch calibration and restarts the
     device, which then comes up as if never configured (lands on the
     Touch Calibration screen with compiled-in defaults).
4. **Touch Calibration screen** — reachable from Settings' Display >
   Touchscreen submenu ("Calibrate"), and shown automatically at first boot
   before any calibration exists.
   - A 5-point calibration: the operator taps 4 corner crosshair targets
     (inset from the true screen edges so they can't be clipped) plus a
     center point used as a precision check.
   - The computed calibration (extrapolated out to the true screen edges)
     is stored in NVS and applied immediately.
   - A "Back"/"Cancel" button is shown only when the screen was entered
     from the Touchscreen submenu for re-calibration (returning there); the
     mandatory first-run flow has no way out, since there is no prior
     calibration to fall back to.
5. **Verify Calibration screen** — reachable from Settings' Display >
   Touchscreen submenu ("Verify Calibration").
   - A full-screen canvas: every touch draws a dot at the exact calibrated
     coordinate (with a live X/Y label), so miscalibration is visible
     directly rather than inferred from misses on real widgets.
   - Navigation is by swipe rather than fixed buttons, since a small
     button is hard to hit precisely under exactly the miscalibration this
     screen exists to diagnose: swipe down from near the top edge to go
     Back (to the Touchscreen submenu), swipe up from near the bottom edge
     to Clear the drawn dots.

## Behavior details

- **Sidetone**: an audible tone plays for the exact duration the key is
  logically down (post-debounce, post-decode-state), at the configured
  frequency, volume, and envelope — this applies identically whether
  driven by the iambic keyer's automatic element timing or by manual
  straight-key timing. The tone is synthesized as a sine wave via the
  ESP32's DAC (not a square wave), with each key-down/key-up transition
  shaped by a raised-cosine attack/decay ramp (the configured envelope
  duration) to avoid audible keying clicks, and scaled by the configured
  volume. The DAC channel stays continuously enabled; between tones it
  simply holds the mid-scale silent sample rather than being powered down —
  power-cycling the channel was tried to cut noise pickup during silence,
  but the power transition itself produced an audible click worse than the
  noise it avoided.
- **Debounce**: raw paddle/key contact closures are debounced in software
  so mechanical contact bounce does not corrupt timing or produce spurious
  short elements. Straight Key mode is always debounced; Iambic Mode
  A/B/Ultimatic debouncing is controlled by the Paddle Debounce setting.
- **Persistence**: WPM, key mode, paddle swap, paddle debounce, sidetone
  frequency, sidetone volume, sidetone envelope, screen brightness,
  brightness auto/manual mode, Practice text size, and touch calibration
  survive power loss. On boot, the last-saved values are loaded and applied
  before the operator can key anything (or interact with the touchscreen, in
  the case of calibration). Settings writes to NVS are debounced by a couple
  of seconds after the last edit, rather than on every tap/step, to reduce
  flash wear.
- **Defaults** (first boot / no saved settings): 15 WPM, Iambic Mode B, no
  paddle swap, paddle debounce on, 600 Hz sidetone, 50% sidetone volume,
  10 ms sidetone envelope, 100% brightness, brightness auto mode off, Small
  Practice text size. No default touch calibration exists — it must be
  created via the first-boot calibration flow.
- **Factory reset**: erases saved keyer settings and touch calibration
  together and restarts the device, so it comes back up exactly as an
  unconfigured device would.

## Explicit non-goals (out of scope for this version)

- No receive/copy practice or quizzing (flashing/playing code for the user
  to identify).
- No Farnsworth spacing (character-speed vs. word-speed timing split).
- No statistics, history, or progress tracking.
- No Wi-Fi, OTA updates, or connectivity of any kind.
