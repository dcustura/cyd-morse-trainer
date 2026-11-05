# Morse Code Keyer & Send Trainer — Functional Specification

## Purpose

A standalone Morse code **keyer and send trainer**: the operator keys Morse
code on a physical paddle (iambic) or straight key wired to the board, and
the device decodes the keying in real time into readable text shown on the
touchscreen.

The project has two goals:

- **Send trainer**: practice *sending* clean, correctly-timed Morse, with
  live decoding as feedback. It does not teach receiving/copying code.
- **CW keyer**: an easy-to-use, full-featured electronic keyer for on-air
  operation, driving a transceiver's key input through the external key
  output (see Hardware below). The reference point is the keyer built into
  modern transceivers such as the QRP Labs QMX, plus standalone keyers such
  as the K1EL WinKeyer.

This spec describes **implemented** behavior only. Planned keyer features
(transmit arm/off-air mode, weight, PTT/hang time,
contest macros, computer keying, …) are tracked in
[`backlog.md`](backlog.md). Each one moves into this spec when it's
implemented.

## Hardware

- Board: ESP32-2432S028R ("Cheap Yellow Display"), ESP32, 2.8" 320×240
  ILI9341 SPI TFT, resistive XPT2046 touch controller, onboard speaker, and
  an onboard LDR (ambient light sensor) used for auto-brightness (see the
  Brightness setting below; non-functional on stock boards — see the
  README's Hardware section).
- External input: a two-lever iambic paddle (or a straight key), wired to
  two GPIOs on the board's header (dit and dah). A straight key is wired to
  the same GPIO used as "dit" in paddle mode.
- External output: a digital key line (3.3 V, active-low, default GPIO17,
  shared with the onboard RGB LED's blue channel) that is on for exactly
  the logical key-down periods produced by the paddle or straight key
  (post-debounce, after the keyer's element timing and Auto Space). It
  switches instantly at each key edge; the sidetone's envelope ramp doesn't
  apply to it. It isn't driven by sidetone-only playback: the Sidetone
  "Test" button and the Morse Reference screen sound the sidetone without
  keying this line. It's only driven while **on-air** (see On-air /
  off-air under Behavior details); off-air it stays off. Wiring requirement: key a transceiver through an
  isolating stage such as an optocoupler, not by connecting the GPIO
  directly to the rig's key jack. The default pin hasn't been confirmed on
  real hardware yet (see `main/Kconfig.projbuild`).

## Boot flow

- If no touch calibration has been saved yet (first boot, or after a
  factory reset), the device boots directly into the **Touch Calibration**
  screen instead of the Main Menu, since touch input isn't usable yet.
- On every other boot, the last-saved touch calibration and keyer settings
  are loaded and applied automatically, and the device starts at the
  **Main Menu**.

## Screens and navigation

1. **Main Menu** — entry point on boot (once calibrated). Three buttons:
   "Send", "Settings", and an on-air quick toggle labeled `Off Air` or
   `On Air` (red while on-air). A tap flips the live on-air state only;
   it isn't saved (see On-air / off-air under Behavior details).
2. **Send screen** — reachable from the Main Menu.
   - Shows a scrolling text area of decoded characters, updated live as the
     operator keys, in one of two sizes set by the Large Text setting
     (Settings' Display submenu): Small (the compact bitmap font, ~26
     characters per visible line at the default width) or Large (a smoother,
     anti-aliased custom monospace font sized for easier reading at a
     distance, fewer characters per line). Changing the setting restyles the
     text area immediately without losing its content.
   - Shows the currently active WPM and key mode as two short lines, e.g.
     `18 WPM` and `Iambic B`, refreshed whenever Settings are saved. Tapping
     it is a shortcut straight into the Settings screen's Keyer submenu (see
     below); that submenu's own Back tile returns here instead of to the
     Settings screen when entered this way.
   - **Quick speed change**: long-pressing the WPM/mode label shows a small
     bar over the top of the text area: `-`, the current WPM, `+`, and a
     close (✕) button. `-`/`+` step WPM by 1 within 5–40 (hold to repeat),
     applying at once and saving it like the Keyer submenu's WPM popup.
     The bar hides after 5 s without a tap, on ✕, or when leaving the
     screen. Releasing the long press doesn't open the Keyer submenu.
   - A small red indicator dot (bottom-right of the screen, vertically
     centered on the Clear/Back buttons) lights up whenever the key/paddle
     is down and the sidetone is sounding, and goes dark when it releases.
   - A **TX** button (between the indicator dot and Back) is an on-air
     quick toggle: red while on-air, and a tap flips the live on-air state
     only, without saving it.
   - An icon-only Memories button (envelope glyph, left of Help) opens the
     **Memories panel** (see below).
   - An icon-only Help button (list glyph, bottom-right, left of the
     indicator dot and Back) opens the **Morse Reference screen** (see
     below).
   - An icon-only Clear button (trash glyph, bottom-left) empties the
     decoded text and resets in-progress decode state.
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
     equivalent — SK, HH, VE, CT — are shown as green `XX` text followed by a
     space (e.g. `SK `), so the color alone marks them as a single
     procedural unit rather than two ordinary letters, and the trailing
     space keeps whatever follows from running into the badge, with two
     exceptions that act as control characters
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
     Sidetone, and Display fill the top row; the bottom row has System, an
     **On Air** toggle, and a Back tile (marked with a left-chevron icon).
     - **On Air**'s switch shows the live on-air state. A tap flips it and
       saves the new state as the one the device boots into; it's the only
       on-air control that saves.
     There is no slider or dropdown anywhere on this screen or its
     submenus, and no OK/Cancel step — every change is applied to the
     running trainer and persisted to NVS the moment it's made (writes are
     briefly debounced so rapid taps or a held `-`/`+` button don't hit NVS
     on every step). Back returns to the Main Menu.
   - Every submenu screen (Keyer/Sidetone/Display/Touchscreen/System)
     follows the same pattern as this top-level grid: no separate title
     bar, and Back (left-chevron icon) is just the last tile in the grid,
     sized N×M with N = M or N = M + 1 so tiles stay wide rather than tall
     and narrow.
   - **Keyer** tile opens a submenu screen with a 3×2 grid of tiles; every
     tile opens a popup to change its value except Paddle Swap and Paddle
     Debounce, which are themselves on/off toggle switches (see below).
     Paddle Swap's tile only appears while Key Mode is Iambic Mode A/B or
     Ultimatic; while Key Mode is Straight Key, a **Ring Input** toggle
     takes its place in the grid instead (see below):
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
       - Straight Key mode ignores the dah GPIO by default; the dit GPIO
         is read directly as raw key-down/key-up with no automatic
         element timing — the operator controls dit/dah length by hand.
         The Ring Input toggle (see below) changes this to also read the
         dah GPIO.
       - Ultimatic mode also uses both paddle GPIOs, but doesn't
         alternate on a live squeeze: whichever paddle was pressed more
         recently keeps repeating for as long as both are held (a
         same-instant first press ties toward dit). Briefly tapping the
         opposite paddle while the primary is still held still latches
         and inserts that tap once, the same squeeze-insert behavior as
         Iambic — but unlike Mode B, nothing is remembered once both
         paddles are released; there is no full-release memory.
     - **Paddle swap**: swaps which physical paddle lever is treated as
       dit vs dah, for operators who prefer the opposite orientation. Like
       Paddle Debounce below, it has no popup: the tile is itself a toggle
       switch, showing the current on/off state (on = Swapped), and
       tapping anywhere on the tile flips it immediately. This setting
       has no effect in Straight Key mode (which always reads the dit/
       "tip" GPIO unswapped — see Ring Input below for reading the other
       conductor instead); its tile isn't shown there at all, replaced by
       Ring Input, but the real stored value is untouched and takes
       effect again as soon as a different Key Mode is selected.
     - **Paddle debounce**: enables/disables software debounce while in
       Iambic Mode A/B/Ultimatic. The tile is itself a toggle switch,
       showing the current on/off state, and tapping anywhere on the tile
       flips it immediately. This setting has no effect in Straight Key
       mode, which is always debounced (a mechanical straight key bounces;
       a clean paddle contact often does not need the extra latency) —
       while Key Mode is set to Straight Key, the tile instead shows the
       switch forced on and greyed out/non-tappable, rather than
       displaying (or letting the operator change) a setting that
       wouldn't actually apply; the real stored value is untouched and
       takes effect again as soon as a different Key Mode is selected.
     - **Ring Input**: Straight Key mode only — takes the Paddle Swap
       tile's place in the grid whenever Key Mode is Straight Key (and
       disappears again, back to Paddle Swap, for every other Key Mode).
       Like Paddle Swap and Paddle Debounce, it's a toggle switch tile
       with no popup. Off (the default) matches Straight Key's normal
       dit-only behavior above. On, the dah GPIO — the "ring" conductor
       on a standard TRS paddle/key jack (tip = dit, ring = dah, sleeve =
       ground) — is also read and ORed into the key signal alongside dit
       ("tip"), so a straight key or cootie key wired across both
       conductors closes the key from either one.
     - **Auto Space** (50–100%): automatic space insertion, an Iambic Mode
       A/B/Ultimatic keying feature (see below for why it has no effect in
       Straight Key mode). Standard PARIS timing requires a 3-unit pause
       to end a character and a 7-unit pause to end a word; if the
       operator touches a paddle again after a *shorter* pause — but
       still at or past this percentage of the standard gap — the keyer
       withholds actually starting the next element until enough real
       silence has accumulated to reach that standard gap, correcting an
       operator who sends without leaving full standard spacing. 100%
       disables this (an element always starts the instant a paddle is
       touched, however short the preceding pause); lower percentages
       withhold starting on a correspondingly shorter pause. A paddle
       touched during the withhold is remembered and started the moment
       the gap target is reached, even if released before then. Its popup
       has `-`/`+` buttons, stepping by 5 points. This is independent of,
       and not the same as, Farnsworth spacing (see Explicit non-goals
       below) — it only pads out real silence the operator already
       started to a standard length, it never slows down an element the
       operator is actively sending. Like Paddle Debounce, it has no
       effect in Straight Key mode (which has no automatic element timing
       for it to act on — see above); while Key Mode is Straight Key, its
       tile instead shows greyed out/non-tappable (`LV_STATE_DISABLED`)
       rather than displaying or letting the operator change a setting
       that wouldn't actually apply, though the real stored value is
       untouched and takes effect again as soon as a different Key Mode
       is selected.
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
     - **Large Text**: switches between the Small and Large decoded-text
       fonts on the Send screen (see the Send screen's description
       above). Like Paddle Swap and Paddle Debounce on the Keyer submenu,
       it has no popup: the tile is itself a toggle switch (on = Large),
       flipped immediately by tapping anywhere on the tile.
   - "System" submenu: a read-only "Firmware" tile showing the firmware
     version (the release tag, e.g. `v1.0.1`, for release builds), the
     "Reset to Defaults" tile below, and Back.
   - "Reset to Defaults" tile (in the System submenu), after a Yes/No confirmation, erases
     the keyer settings, the touch calibration and the message memories
     (with their tile assignments) and restarts the
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
6. **Morse Reference screen** — reachable from the Send screen's Help
   button. No header: the whole screen is a row of tabs — "A-Z", "0-9",
   "/?", Prosigns, and "Back" as a same-sized fifth tab-bar slot (tapping
   it returns to the Send screen, stopping any tone still sounding,
   rather than switching to a tab) — each showing a grid of large tappable
   buttons filling the rest of the screen.
   - Each tab's grid holds every decodable character in that category (the
     same character set as the Send screen: the 26 letters, 10 digits,
     standard Morse punctuation `. , ? ' ! / ( ) & : ; = + - _ " $ @`, and
     the 8 prosigns SK/HH/VE/CT/BT/AR/KN/AS), with every button on a tab the
     same size (a category whose count doesn't divide evenly into rows gets
     a blank filler slot rather than uneven button widths). Only one tab's
     grid is on screen at a time, so its buttons are large and easy to hit.
     Tabs switch only by tapping the tab bar; swiping the grid does not
     change tabs.
   - Deliberately no on-screen readout of which character or pattern was
     last played: the screen is meant for learning each code by ear, not by
     sight.
   - Tapping a button plays that character's Morse code audibly (dit/dah
     timing at the trainer's currently configured WPM, so it always sounds
     like the speed being practiced) once, and highlights the tapped button
     until a different one is tapped. Holding a button down doesn't repeat
     it, and any tap - the same button or a different one - is ignored
     while a tone is still sounding, so plays can never overlap; the next
     tap is only honored once the current character has finished playing.
7. **Memories panel** — reachable from the Send screen's Memories button.
   A 3×3 tile grid in the Settings style, with a one-line sending strip
   below it:

   ```
   ┌────────┬────────┬────────┐
   │ CQ    ▶│ 5NN    │ QRZ?  1│   <- "1": queue position badge
   │ #01    │ #02    │ #05    │
   ├────────┼────────┼────────┤
   │ 73     │ TEST   │ (none) │
   │ #03    │ #04    │        │
   ├────────┼────────┼────────┤
   │Repeat: │  Stop  │  Back  │
   │  ON    │        │        │
   ├────────┴────────┴────────┤
   │CQ CQ DE Y[O]8ABC <AR> 2/3 │   <- sending strip
   └──────────────────────────┘
   ```

   - **Six memory tiles**, each assigned one library memory (see Behavior
     details). A tile shows the memory's name in large text, or the start
     of its text if it has no name, with `#NN` small underneath. A tile
     with no assignment shows `(none)`; an assigned memory with empty text
     shows `#NN (empty)`. Tapping either does nothing.
   - **Tap** a tile: sends its memory, or queues it if a message is already
     playing. Tapping the tile that's playing, or one already queued, does
     nothing. Queued tiles show their queue position (`1`, `2`, …) in the
     top-right corner. With the queue full, the tap does nothing and the
     sending strip shows `Queue full` for 2 s.
   - The playing tile is highlighted (amber) with a `▶` in its top-right
     corner.
   - **Long-press** a tile: opens the **memory picker**, a full-screen
     scrollable list laid over the panel: a "None" row, then all 32
     memories (`#NN`, name, start of the text), with the current
     assignment highlighted, and a Cancel button. Tapping a row assigns
     that memory to the tile (saved to NVS) and closes the picker. If the
     save fails, the tile keeps its old memory and the sending strip shows
     `Save failed` for 2 s.
     Long-press is ignored on the tile that's playing or queued.
   - **Session repeat state**: each tile keeps its own repeat on/off, count
     and pause for the current visit to the panel. It starts as a copy of
     its memory's defaults the first time the tile is played in the visit
     (or, for a message still playing from an earlier visit, from what it's
     playing with), and is discarded when the panel is opened again. The
     memory itself is never changed by it.
   - **Repeat tile**: shows `Repeat: ON` or `Repeat: OFF` for the current
     tile, the one playing or else the one played most recently in this
     visit. It's greyed out and inactive until a tile has played in the
     visit.
     - Tap toggles repeat for the current tile. Turning it on during a
       single send makes that send repeat; turning it off during a send
       lets the send finish, then stops; turning it off during a pause
       stops at once.
     - Long-press opens a popup to change the current tile's **Count** (0,
       shown as "No limit", to 99) and **Pause** (1–99 s) with `-`/`+`
       buttons (hold to repeat), for this visit only, plus a "Memory
       defaults" button that restores the memory's own values and Close.
       Changes apply at once to the message playing on that tile. A
       pause changed while it runs counts from the pause's start; if that
       shorter pause has already run out, the next send starts at once.
   - **Stop** tile: stops playback at once and clears the queue, the same
     as a paddle cancel.
   - **Sending strip**: while a message plays, shows its text, scrolled so
     the character being sent stays in view and highlighted (amber), and
     on the right the progress while repeating: `2/5` (send 2 of 5), or `2/`
     followed by a loop icon with count 0, plus a seconds countdown during
     a pause (`2/5 6s`). Empty when idle.
   - **Back** returns to the Send screen. Playback continues, so the sent
     text can be watched there being decoded, and a paddle touch still
     cancels it; the playing message keeps the repeat values it started
     with.

## Serial CLI

- The device exposes a line-based command console on the same UART already
  used for the ESP-IDF log output and `idf.py monitor` (115200 baud) — no
  separate hardware or connection is needed. Prompt: `morse> `.
- Eight commands, chosen for scripting: every response ends with exactly one
  `OK` or `ERR <reason>` line, so a script can tell where a response ends
  even though log lines from elsewhere in the firmware may appear between
  commands.
  - `get <key>` — prints `key=value` for one setting; `get settings` prints
    every setting, one `key=value` line each.
  - `set <key> <value>` — validates/clamps the value exactly like the
    Settings screen does, applies it to the running trainer immediately
    (keeping the Settings screen's tiles in sync so they don't show a stale
    value next time it's opened), persists it to NVS right away, and echoes
    back the value actually applied (which may differ from what was typed,
    if it was out of range).
  - `save` — persists the current settings to NVS (normally unnecessary,
    since `set` already saves).
  - `reset` — erases saved settings, the same as the Settings screen's
    "Reset to Defaults" minus the touch-calibration and message-memory
    erase and automatic restart; a manual restart is required afterward
    for the defaults to take effect.
  - `mem` — reads and edits the message memories (see Behavior details),
    saving each change to NVS at once:
    - `mem list` — one line per memory with a name or text:
      `NN name="..." text="..." repeat=on|off count=N pause=N`.
    - `mem get <N>` — the same line for memory N (1–32), even if empty.
    - `mem set <N> <field> <value>` — field is `name`, `text`, `repeat`
      (`on`/`off`), `count` (0–99) or `pause` (1–99, seconds). Name and
      text are validated and normalized (see Behavior details); an invalid
      value is rejected with `ERR` naming the problem and its 1-based
      position. Echoes the memory's new line.
    - `mem clear <N>` — resets memory N to the empty default.
    - `mem tile <T> <N|none>` — assigns memory N, or nothing, to memories
      panel tile T (1–6), echoing `tileT=NN` or `tileT=none`.
    - `mem tiles` — lists the six tile assignments that way.
    - `mem play <N>` — plays memory N with its own repeat settings, or
      queues it if a message is already playing (see Behavior details).
      Prints `play=started` or `play=queued`; `ERR` if the memory is empty,
      already playing, already queued, or the queue is full.
    - `mem stop` — stops playback and clears the queue.
    - Quote values that contain spaces. Inside quotes, `\"` is a literal
      `"`. `mem list` and `mem get` print name and text the same way, so
      their output can be pasted back into `mem set`.
  - `send <text>` — keys text once (no repeat), the same way as a
    message memory: started at once, or queued behind a playing message
    (see Playing memories under Behavior details), and cancelled by a
    paddle touch or `mem stop`. Words may be given unquoted
    (`send CQ CQ DE X <AR>`, up to 29 words; more is refused with `ERR`,
    since the console would silently drop the rest) or quoted. The text is validated and
    normalized like a memory's text (up to 80 characters, prosigns in
    `<..>`). Prints `send=started` or `send=queued`; `ERR` for invalid
    text, nothing to send, or a full queue. Sending the same text again
    queues another copy.
  - `tx [on|off]` — shows (`tx=on|off`) or changes the live on-air state,
    like the touchscreen's quick toggles: not saved, so the next boot
    returns to the saved `on_air` setting.
  - `log <none|error|warn|info|debug|verbose>` — sets the runtime log
    verbosity for every module, so a script can quiet interleaved log
    output before sending `get`/`set` commands.
- Keys, matching the Settings screen's Keyer/Sidetone/Display submenus: `wpm`, `keymode` (`a`/`b`/`straight`/`ultimatic`),
  `paddle_swap`, `paddle_debounce`, `straight_ring_input`,
  `auto_space_pct` (50–100), `tone_hz`, `volume_pct`, `envelope_ms`,
  `brightness_pct`, `brightness_auto`, `send_large_text`, and `on_air` (all
  booleans as `on`/`off`). `get on_air` shows the saved boot value, which
  can differ from the live state (`tx`); `set on_air` changes both. `straight_ring_input` can
  be set regardless of the current `keymode`, but (like the Settings
  screen's Ring Input tile) only takes effect while `keymode` is
  `straight`. Setting `brightness_pct` while `brightness_auto` is on
  switches back to manual first, the same as stepping `-`/`+` on the
  Brightness popup.

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
- **Auto Space**: an Iambic Mode A/B/Ultimatic keying feature (the keyer
  that turns paddle contacts into dit/dah/gap timing, not the decoder
  that reads it back). After an element's standard 1-unit trailing gap,
  if the operator touches a paddle again before a full 3-unit pause
  (character) or 7-unit pause (word) — but at or past this setting's
  percentage of that standard gap — the keyer withholds actually
  starting the new element until enough real silence has accumulated to
  reach the standard gap, rather than starting it the instant it's
  touched. This shapes the *actual* generated signal (sidetone, external
  key line, and whatever decodes it) to standard spacing even when the
  operator's own pause fell short, rather than just reading sloppy timing
  leniently. A touch that comes during the withhold is remembered and
  started the moment the gap target is reached, even if the paddle was
  released again before then. 100% (the default) disables this outright
  (an element always starts the instant a paddle is touched, however
  short the preceding pause); 70%, for example, withholds whenever the
  pause is at least 2.1 units (character) or 4.9 units (word). A pause already below the
  threshold, or already at or past the standard gap on its own, is left
  alone — nothing is ever slowed down while the operator is actively
  sending. The character-gap threshold is never allowed below 2 units
  regardless of the setting, so it can't be confused with the normal
  1-unit gap between elements of the same character. Straight Key mode
  has no automatic element timing for this to act on (the operator's own
  dit/dah/gap timing is read directly, unmodified), so Auto Space has no
  effect there.
- **Persistence**: WPM, key mode, paddle swap, paddle debounce, ring
  input, Auto Space percentage, sidetone frequency, sidetone volume,
  sidetone envelope, screen brightness, brightness auto/manual mode,
  Send screen text size, the on-air setting, and touch calibration survive power loss. On boot, the last-saved values are
  loaded and applied before the operator can key anything (or interact
  with the touchscreen, in the case of calibration). Settings writes to
  NVS are debounced by a couple of seconds after the last edit, rather
  than on every tap/step, to reduce flash wear.
- **Defaults** (first boot / no saved settings): 15 WPM, Iambic Mode B, no
  paddle swap, paddle debounce on, ring input off, Auto Space off (100%), 600 Hz
  sidetone, 50% sidetone volume, 10 ms sidetone envelope, 100% brightness,
  brightness auto mode off, Small Send screen text size, off-air. No default touch calibration
  exists — it must be created via the first-boot calibration flow.
- **Message memories**: a library of 32 memories, numbered 1–32, stored in
  their own NVS namespace. Each has a name (up to 8 characters, may be
  empty), a text (up to 80 characters, may be empty), a repeat default
  (on/off), a repeat count (0–99, the total number of transmissions
  including the first; 0 means until stopped) and a pause (1–99 s between
  transmissions). Defaults: empty name and text, repeat off, count 0,
  pause 10 s. Six memories panel tile assignments are stored with them,
  defaulting to tiles 1–6 → memories 1–6; the same memory may be on more
  than one tile. Memories are played from the Memories panel or with
  `mem play`, and edited only over the serial CLI (`mem`).
  - Text is converted to uppercase, runs of spaces become one, and
    leading/trailing spaces are dropped. Allowed: A–Z, 0–9, space, the
    punctuation `. , ? ' ! / ( ) & : ; = + - _ " $ @`, and the eight
    prosigns the decoder knows written in angle brackets (`<AR>`, `<SK>`,
    `<KN>`, `<BT>`, `<HH>`, `<VE>`, `<CT>`, `<AS>`, any case). Anything
    else, an unclosed `<`, an empty `<>`, or any other bracketed sequence
    is rejected. The length limit applies after normalization, with a
    prosign counting as its 4 characters. Names follow the same rules
    without prosigns.
- **Playing memories**:
  - A message is sent at the current keyer WPM with standard PARIS
    timing: dit 1 unit, dah 3, element gap 1, character gap 3, word gap 7.
    A prosign's letters run together with element gaps only. A WPM change
    applies from the next element or gap. Auto Space, Paddle Swap and the
    key mode don't affect it.
  - While a message plays it replaces the keyer as the key source, so the
    sidetone, key output, keying indicator dot and decoder all follow it,
    and the sent text appears decoded on the Send screen. A message
    started just after the operator lets go (while the keyer is still
    finishing an element, or less than a word gap after their last
    key-down) cuts off any element in progress and starts a word gap
    after that last key-down, so it never joins or runs into the
    operator's sending. One started while a paddle or the key is still
    held is cancelled straight away by that contact (see Cancel).
  - **Cancel**: touching either paddle or the straight key (with or
    without Ring Input) while a message is sending or pausing stops it at
    once: a dit or dah in progress is cut short, remaining repeats and the
    queue are cleared, and the touch itself is not keyed. Paddle input is
    then ignored until both paddles are released.
  - **Repeat**: with repeat on, the message is sent `count` times in total
    (0 = until stopped), with `pause` seconds of silence between sends.
  - **Queue**: up to 6 more messages can wait behind the playing one. A
    message already playing or queued isn't queued again. Once something
    is queued, a repeating message ends after its current send (or at
    once, during a pause), and the next message follows after a word gap.
  - Morse Reference playback and the Sidetone "Test" button are ignored
    while a message plays. A message started during either one (e.g. by
    `mem play`) ends it once the reference element or 300 ms test beep in
    progress finishes, and then has the sidetone to itself.
- **On-air / off-air**: the external key output is driven only while
  on-air. Off-air, everything else works unchanged (sidetone, keying
  indicator, decoding, message playback), so the device can be used as a
  trainer while still wired to a rig. Going off-air during an element
  releases the key output at once; going on-air during an element doesn't
  key the rest of it, and the output starts with the next key-down.
  - While on-air, a red frame is drawn around the edge of every screen
    (including popups), so the live state is visible wherever the operator
    is. It doesn't block touches.
  - The saved `on_air` setting sets the state at boot (off-air on a new
    device or after a factory reset). Changing it (the Settings screen's
    On Air tile, or `set on_air`) saves it and applies it at once. The
    quick toggles (Send screen TX button, Main Menu button, `tx` on the
    CLI) change only the live state, so a reboot returns to the saved
    one.
- **Factory reset**: erases saved keyer settings, touch calibration and
  message memories together and restarts the device, so it comes back up exactly as an
  unconfigured device would.

## Explicit non-goals (out of scope for this version)

These are out of scope for both the trainer and the keyer. Anything that
*is* planned lives in [`backlog.md`](backlog.md) instead.

- No receive/copy practice or quizzing (flashing/playing code for the user
  to identify).
- No Farnsworth spacing (character-speed vs. word-speed timing split).
- No statistics, history, or progress tracking.
- No Wi-Fi or OTA updates. Any other connectivity (serial computer keying,
  Bluetooth) is decided in the backlog, not here. Bluetooth isn't built
  into the firmware at all at the moment.
- No transceiver functions (receiver, RF output, frequency control/CAT).
  The device is a keyer that drives an external rig's key input; it isn't a
  radio.
