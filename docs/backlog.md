# Feature Backlog

Planned features for turning the send trainer into a full CW **keyer + send
trainer**: something you can wire to a transceiver's key jack and operate on
the air with, while keeping everything the trainer does today.

How this file works:

- This file lists only work that **has not been built yet**.
  `docs/functional-spec.md` describes what the firmware does today. When a
  feature here is implemented, describe its final behavior in the functional
  spec and **delete its entry here** in the same change. Don't leave
  "done" markers behind.
- Entries are grouped by priority tier and are roughly ordered within each
  tier. Re-order, split, merge or drop entries as plans change.
- Each entry says what the feature is, why it's wanted, and any known design
  constraints. Exact UI layout and setting ranges are decided when it's
  built, then recorded in the spec.
- Reference points cited below: the QRP Labs QMX transceiver's built-in keyer
  (operating manual, firmware 1.03_002), the K1EL WinKeyer, and the
  open-source K3NG Arduino keyer.

## Tier 1: everyday operating, no new hardware

### Message memories

**Status: detailed spec agreed in discussion, implementation in progress.**
Editing on the device is out of scope here (see "Edit memories by keying
them" below). In this step, memories are edited over the serial CLI only.

#### Memory library

- A library of **32 memories**, numbered 1–32 (shown as `#01`–`#32`).
  Each memory has:
  - **Name**: optional, up to 8 characters, shown large on its tile, e.g.
    `CQ`, `RIG`, `73`.
  - **Text**: up to 80 characters, the message that gets sent.
  - **Repeat default**: on/off, whether this memory repeats when played.
  - **Repeat count**: 0–99, the total number of transmissions when
    repeating. 0 means repeat until stopped.
  - **Pause**: 1–99 s of silence between transmissions when repeating.
- Defaults: every memory is empty (no name, no text), with repeat off,
  count 0 and pause 10 s.
- Text and names are stored in uppercase; lowercase input is converted.
- Allowed text characters are exactly the ones the decoder knows: A–Z,
  0–9, space, the punctuation `. , ? ' ! / ( ) & : ; = + - _ " $ @`, and
  the eight prosigns the decoder knows, written in angle brackets: `<AR>`,
  `<SK>`, `<KN>`, `<BT>`, `<HH>`, `<VE>`, `<CT>`, `<AS>`. The letters inside
  brackets are sent with no gap between them. Any other character, any
  other bracketed sequence (e.g. `<SOS>`, which the decoder would show as
  `*`), an unclosed `<`, or an empty `<>` is rejected when the text is
  set. Names allow the same characters, without prosigns.
- Spacing: a space is a word gap (7 units). A run of spaces counts as one
  word gap, and leading/trailing spaces are dropped.
- Memories persist in NVS, in their own namespace, separate from the
  keyer settings.

#### Sending

- A message is sent at the **current keyer WPM** with standard PARIS
  timing: dit 1 unit, dah 3, element gap 1, character gap 3, word gap 7.
  If WPM changes mid-message, the change applies from the next element.
  Auto Space and Paddle Swap don't apply. Key mode doesn't matter: a
  message plays the same in every key mode.
- Playback is a second key source in the same 1 ms paddle task as the
  keyer. While a message plays, its key state replaces the keyer's, so the
  sidetone, external key output, keying indicator dot and decoder all
  behave exactly as for paddle keying. The sent text is therefore also
  decoded into the Send screen's text area, including prosign handling
  (e.g. `<AR>` decodes to `+` and forces a line break).
- **Cancel**: touching either paddle (or the straight key, with or without
  Ring Input) during sending *or* during a repeat pause stops at once. A
  dit or dah in progress is cut short (key up immediately), any remaining
  repeats and the whole queue are cleared, and the touch itself is **not
  keyed**. Paddle input is ignored until both paddles are released, and
  keying works normally from the next press.
- The on-screen **Stop** tile does the same as a cancel.
- Morse Reference screen playback is ignored while a message is playing,
  since both use the sidetone.

#### Repeat

- Each tile keeps a **session repeat state** for the current visit to the
  memories panel: repeat on/off, count and pause. It starts as a copy of
  its memory's defaults the first time the tile is played in that visit,
  and is discarded when leaving the panel. The memory itself is never
  changed by this.
- When repeating, transmission N+1 starts after the pause. The count
  includes the first transmission, so count 3 sends 3 times in total.
- The **Repeat tile** shows and controls the session repeat state of the
  *current tile*: the one playing, or else the one played most recently in
  this visit. It shows `Repeat: ON` or `Repeat: OFF`, and is greyed out
  until a tile has been played in this visit.
  - Tap: toggles repeat on/off for the current tile.
    - Turning it on during a single send makes that send repeat once it
      finishes.
    - Turning it off during a send lets that transmission finish, then
      stops.
    - Turning it off during a pause stops at once.
  - Long-press: opens a popup to override the current tile's **count**
    (0 = until stopped) and **pause** for this visit only, using `-`/`+`
    buttons, plus a "Memory defaults" button that restores the memory's own
    values.

#### Queue

- Tapping a tile while another message is playing (or pausing) **queues**
  it after the current one. Queued tiles show their queue position as a
  small badge. Up to 6 entries can be queued; further taps are ignored.
- When something is queued, a repeating message finishes its current
  transmission and then ends its repeats. The next queued message follows
  after a word gap (7 units), with no pause.
- Tapping the tile that's currently playing does nothing.
- Stop and a paddle cancel clear the queue (see Sending).

#### Memories panel (screen)

- Opened from a new icon-only button on the Send screen's bottom row,
  left of Help and the same size as Help.
- A full-screen 3×3 tile grid in the Settings style, plus a one-line
  sending strip at the bottom:

  ```
  ┌────────┬────────┬────────┐
  │ CQ  ▶  │ 5NN TU │ NAME   │
  │ #01    │ #02    │ #07  1 │   <- "1": queue position badge
  ├────────┼────────┼────────┤
  │ RIG    │ 73     │ (none) │
  │ #12    │ #03    │        │
  ├────────┼────────┼────────┤
  │Rpt: ON │  Stop  │  Back  │
  ├────────┴────────┴────────┤
  │CQ CQ DE Y[O]8ABC K  2/5·6s│   <- sending strip
  └──────────────────────────┘
  ```

- **Six memory tiles**, each assigned one library memory. A tile shows the
  memory's name in large text, or the start of its text if it has no name,
  with `#NN` small underneath. A tile with no assignment shows `(none)`,
  and an assigned memory with empty text shows `#NN (empty)`. Tapping
  either of those does nothing.
- Tile assignments persist in NVS. Defaults are tiles 1–6 → memories
  #01–#06. The same memory may be assigned to more than one tile.
- **Tap** a tile: sends its memory, or queues it if something is already
  playing (see Queue).
- **Long-press** a tile: opens the **memory picker**, a full-screen
  scrollable list of all 32 memories, one row each showing `#NN`, the name
  and the start of the text, plus a "None" row and Cancel. Tapping a row
  assigns that memory to the tile and closes the picker. Long-press is
  ignored on the tile that's playing or queued.
- The playing tile is highlighted, with a `▶` marker.
- **Sending strip**: shows the text of the message being sent, scrolled so
  the current character is visible and highlighted, plus progress: `2/5`
  while repeating with a count, `2/∞` with count 0, and a seconds countdown
  during a pause. It's empty when idle.

#### Leaving the panel

- Back returns to the Send screen. Playback **continues**, so you can
  watch the decoded text, and a paddle touch still cancels it. Session
  repeat states are discarded, but the message already playing keeps the
  repeat values it started with.

#### Serial CLI

- A new `mem` command; every response ends with `OK` or `ERR <reason>`,
  like the existing commands:
  - `mem list`: one line per non-empty memory: `NN name="..." text="..."
    repeat=on|off count=N pause=N`.
  - `mem get <N>`: the same line for one memory.
  - `mem set <N> <field> <value>`, where field is `name`, `text`, `repeat`
    (`on`/`off`), `count` (0–99) or `pause` (1–99). Text and names are
    validated (see Memory library) and rejected with `ERR` if invalid.
    Quote values that contain spaces. Inside a quoted value, `\"` is a
    literal `"` (`\` isn't an allowed character, so it needs no escape of
    its own). `mem list` and `mem get` print `name` and `text` the same
    way, so their output can be fed back to `mem set` unchanged.
  - `mem clear <N>`: resets one memory to the empty default.
  - `mem tile <T> <N|none>`: assigns memory N (or nothing) to tile T (1–6).
  - `mem tiles`: lists the six tile assignments.
- Changes apply to the panel at once and are saved to NVS.

#### Reset

- Settings > System > Reset to Defaults also erases all memories and tile
  assignments, since it is a full factory reset. The CLI `reset` command
  erases keyer settings only and leaves memories alone.

#### Implementation notes

- `components/morse_codec`: text validation/normalization and a token
  encoder (`morse_text.h`) are done. They yield each character's dit/dah
  pattern and whether a word gap precedes it; the player turns that into
  timing, so a WPM change applies from the next element.
- New hardware-independent component (e.g. `message_player`): plays a
  message by returning key_down for a given `now_ms`, like
  `iambic_keyer_service()`. It handles repeat count/pause, the queue,
  cancel, and progress reporting for the strip, with host tests.
- `main/`: NVS store for memories and tile assignments, the panel and
  picker screens, the Send screen button, CLI `mem` command, and
  integration into `paddle_task`: cancel on paddle contact, then ignore
  paddle input until both paddles are released.

### Edit memories by keying them

- QMX lets you enter message text by sending it on the paddle, with its
  decoder doing the typing. Here the decoder is already the core of the
  product. An "edit memory" mode that records decoded text avoids an
  on-screen keyboard on a 320×240 resistive screen.
- Must cover every field of a memory (name, text, repeat default, count,
  pause), not just the text. Until then, editing is CLI-only (see Message
  memories).
- Open question: how corrections work. On the Send screen, HH deletes
  the whole last word (or a `*` placeholder), not one character. Either
  reuse that word-level HH, or add a per-character delete (an on-screen
  backspace, or a different prosign) for fixing a single wrong letter in a
  memory.

### Transmit arm / Off-air mode

- A clearly visible on-air vs off-air toggle, reachable without opening the
  Settings screen. In off-air mode the external key output (and future
  PTT output) is never driven. Sidetone and decoding still work.
- This keeps trainer use safe while the board stays wired to a rig. QMX has
  the same feature (called "Practice mode" there) for the same reason.
- Open question: whether the toggle persists across reboots, or the device
  always boots into off-air mode to be safe.

### Quick speed change

- Change WPM from the Send screen in one gesture (QMX: one button press,
  then turn the knob), without going through Settings > Keyer > WPM. For
  example, long-press the existing WPM/mode label to show `-`/`+` controls
  over the text area.
  - Constraint: a tap on that label already opens the Keyer submenu, via an
    `LV_EVENT_CLICKED` handler in `main/ui_send.c`. LVGL fires CLICKED
    on release even after a long press, so the existing handler has to move
    to `LV_EVENT_SHORT_CLICKED`. Otherwise a long press would show the
    overlay and then jump into Settings on release.
- Optional hardware add-on, as in K3NG: a speed potentiometer or rotary
  encoder on a free header pin. GPIO35 (input-only, analog-capable) is a
  candidate, but confirm it on the board first.

### Tune

- A Tune button that holds the key output down continuously, for antenna
  tuning, with a safety timeout (on the order of 10–20 s) and cancel on any
  touch or paddle press. QMX uses "speed 0 means straight key" for the same
  purpose. A dedicated button is clearer on a touchscreen.
- Never active in off-air mode.

## Tier 2: keyer timing for experienced operators

### Weight (dit/dah ratio)

- Make key-down elements longer or shorter than standard (QMX allows 5–95%
  with 50% as standard; WinKeyer allows 25–75%). The extra key-down time is
  taken from the following gap, so overall speed doesn't change. Some
  operators use this for a "heavier" or "lighter" sound, or to suit a
  particular rig.
- Belongs in `components/iambic_keyer`, with host tests. It must also apply
  to memory playback, and the decoder must still read weighted keying
  correctly.

### PTT output with lead-in / tail (semi-QSK hang time)

- A second output, separate from the key line, that switches the
  transmitter or an amplifier: it turns on a configurable lead-in time
  before the first element and stays on for a hang time after the last one.
  - QMX hang-time presets: Auto (8 dits, just over a word gap), Contest (6.1
    dits), and Custom (in dits, tenths of a dit, or ms), clamped to
    1–999 ms.
  - WinKeyer offers separate lead-in and tail settings.
- Hardware: needs another free GPIO. GPIO4 (the RGB LED's red channel on
  community pinouts) is already used as TFT reset on this board. GPIO16
  (green channel) is the likely candidate but is unconfirmed. Add it as a
  Kconfig option like `MORSE_KEY_OUTPUT_GPIO`, with the same "verify on your
  unit" warning.

### Keying compensation / first-element extension

- WinKeyer-style timing corrections for rigs whose QSK switching shortens
  every element or clips the first dit: a fixed number of ms added to every
  key-down, and extra time added to the first element of a transmission
  only.

### Bug mode

- Emulate a semi-automatic "bug" key, as in K3NG: the dit paddle makes
  automatic dits, and the dah paddle is keyed manually like a straight key.
  This would be a new key mode alongside Iambic A/B, Ultimatic and Straight.

## Tier 3: contest and computer integration

### Macros: callsign and serial numbers

- Placeholders inside memories, as in K3NG/WinKeyer: a stored callsign, and
  a contest serial number that can be sent as-is or sent and then
  incremented. Optionally send numbers as "cut numbers" (e.g. T for 0, N
  for 9).
- A small serial-number display or control on the Send screen, so the
  operator can see and correct the next number to send.

### CLI `send` / `stop`

- `send <text>` queues text for transmission through the memory-playback
  engine (same prosign marker, same cancel-on-paddle rule). `stop` aborts
  it. This makes the device scriptable as a keyer from a PC with no extra
  protocol.

### WinKeyer protocol emulation

- Speak the K1EL WinKeyer serial protocol (as K3NG does) so N1MM, fldigi
  and most logging programs can use the device as their CW keyer.
- Constraint: UART0 already carries ESP-IDF logs and the `morse>` CLI. WinKeyer
  mode would have to be a switchable serial mode that silences logs and the
  CLI while active. Decide how to switch back (setting, reboot, or a
  WinKeyer admin command).
- Keying via the USB-serial adapter's DTR line, as QMX does, probably isn't
  usable here. On this board DTR/RTS drive the auto-reset/boot circuit.

### Beacon / fox mode

- Repeat one memory forever with a fixed interval, e.g. for a beacon or
  foxhunt transmitter. This mostly falls out of message memories (repeat
  count 0). It may only need a dedicated entry point and a clear on-screen
  "beacon running" indication.

### Bluetooth

- Bluetooth is currently off: the earlier BLE HID keyboard proof of
  concept (decoded text typed into a phone/PC) was removed. What Bluetooth
  should be used for is still open, likely one of the computer-keying or
  typed-sending ideas in this tier. See `docs/architecture.md` ("Build
  system") for what the proof of concept taught about RAM and boot order.

### Typed sending with a buffer

- Send text typed on a keyboard, with a type-ahead buffer (K3NG supports
  PS/2 and USB keyboards). On this board the realistic routes are the
  serial CLI (covered by `send` above) or a BLE keyboard, with the device
  connecting to the keyboard as a BLE host. Low priority given the screen size.
