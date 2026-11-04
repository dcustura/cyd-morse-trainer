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

- Store several messages (QMX has 12 × 50 characters), e.g. a CQ call or a
  name/QTH exchange, and send one at the current keyer speed through the
  same path as paddle keying (sidetone, key output, decoder, BLE HID).
- Show memories as large tappable buttons reachable from the Practice
  screen. Tap to send once; a second gesture such as a long press repeats
  the message.
- Repeat settings: a repeat count (QMX: 0–99, where 0 means forever) and an
  interval in seconds between repeats, used for a repeated CQ with listening
  gaps.
- Touching either paddle (or the straight key) **cancels sending
  immediately** and hands control back to the operator, which is how you
  answer a station that replies to a CQ. A Stop button on screen does the
  same.
- Prosigns inside a message: use a marker meaning "send the next two
  characters with no gap" (QMX uses `_`, e.g. `_AR`, `_KN`, `_SK`). This
  needs an encoder (text to elements) alongside the existing decoder in
  `components/morse_codec`. That encoder is also the basis for the CLI
  `send` command and WinKeyer support below.
- Persist memories in NVS.

### Edit memories by keying them

- QMX lets you enter message text by sending it on the paddle, with its
  decoder doing the typing. Here the decoder is already the core of the
  product. An "edit memory" mode that records decoded text avoids an
  on-screen keyboard on a 320×240 resistive screen.
- Open question: how corrections work. On the Practice screen, HH deletes
  the whole last word (or a `*` placeholder), not one character. Either
  reuse that word-level HH, or add a per-character delete (an on-screen
  backspace, or a different prosign) for fixing a single wrong letter in a
  memory.
- Also allow editing over the serial CLI (e.g. `set msg1 "CQ CQ DE ..."`).

### Transmit arm / Practice mode

- A clearly visible on-air vs practice toggle, reachable without opening the
  Settings screen. In practice mode the external key output (and future
  PTT output) is never driven. Sidetone, decoding and BLE HID still work.
- This keeps trainer use safe while the board stays wired to a rig. QMX has
  the same "Practice mode" for the same reason.
- Open question: whether the toggle persists across reboots, or the device
  always boots into practice mode to be safe.

### Quick speed change

- Change WPM from the Practice screen in one gesture (QMX: one button press,
  then turn the knob), without going through Settings > Keyer > WPM. For
  example, long-press the existing WPM/mode label to show `-`/`+` controls
  over the text area.
  - Constraint: a tap on that label already opens the Keyer submenu, via an
    `LV_EVENT_CLICKED` handler in `main/ui_practice.c`. LVGL fires CLICKED
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
- Never active in practice mode.

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
- A small serial-number display or control on the Practice screen, so the
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

### Rework Bluetooth

- The current BLE HID keyboard output (decoded text typed into a phone/PC)
  is a proof of concept. Expect to replace or rework it, probably toward
  the keyer features in this tier (computer keying, typed sending). Don't
  build new features on its current behavior.

### Typed sending with a buffer

- Send text typed on a keyboard, with a type-ahead buffer (K3NG supports
  PS/2 and USB keyboards). On this board the realistic routes are the
  serial CLI (covered by `send` above) or a BLE keyboard, which would mean
  connecting to the keyboard as a BLE host, the reverse of today's BLE HID
  role. Low priority given the screen size.
