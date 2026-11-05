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

### Edit memories by keying them

- QMX lets you enter message text by sending it on the paddle, with its
  decoder doing the typing. Here the decoder is already the core of the
  product. An "edit memory" mode that records decoded text avoids an
  on-screen keyboard on a 320×240 resistive screen.
- Must cover every field of a memory (name, text, repeat default, count,
  pause), not just the text. Until then, editing is CLI-only (`mem`, see
  `docs/functional-spec.md`).
- Open question: how corrections work. On the Send screen, HH deletes
  the whole last word (or a `*` placeholder), not one character. Either
  reuse that word-level HH, or add a per-character delete (an on-screen
  backspace, or a different prosign) for fixing a single wrong letter in a
  memory.
- Message variables (`{MYCALL}` etc., see `docs/functional-spec.md`)
  need covering too: braces have no Morse code, so inserting a variable
  into a memory needs another way (e.g. a picker of defined variables),
  and variable values (`var`) are CLI-only as well.

## Tier 2: keyer timing for experienced operators

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
- Never driven off-air, like the key output (`components/key_gate`).

### Keying compensation / first-element extension

- WinKeyer-style timing corrections for rigs whose QSK switching shortens
  every element or clips the first dit: a fixed number of ms added to every
  key-down, and extra time added to the first element of a transmission
  only.

## Tier 3: contest and computer integration

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
  foxhunt transmitter. Message memories already do this (repeat on, count
  0). It may only need a dedicated entry point and a clear on-screen
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
  serial CLI (already possible line by line with `send`, see `docs/functional-spec.md`) or a BLE keyboard, with the device
  connecting to the keyboard as a BLE host. Low priority given the screen size.

## Tier 4: low priority

### Tune

- A Tune button that holds the key output down continuously, for antenna
  tuning, with a safety timeout (on the order of 10–20 s) and cancel on any
  touch or paddle press. QMX uses "speed 0 means straight key" for the same
  purpose. A dedicated button is clearer on a touchscreen.
- Never active off-air (see On-air / off-air in `docs/functional-spec.md`):
  gate it through the same `key_gate` path as normal keying.

### Speed knob

- Optional hardware add-on, as in K3NG: a speed potentiometer or rotary
  encoder on a free header pin, for changing WPM without the touchscreen
  (the on-screen quick speed bar is in `docs/functional-spec.md`). GPIO35
  (input-only, analog-capable) is a candidate, but confirm it on the board
  first.

### Macros: contest serial numbers

- A contest serial number placeholder in memories, sent as-is or sent and
  then incremented (e.g. `5NN {NR+}`), as in K3NG/WinKeyer. Optionally
  leading zeros (001) and "cut numbers" (e.g. T for 0, N for 9).
- A small serial-number display or control on the Send screen, so the
  operator can see and correct the next number to send.
- Low priority: only contests with serial-number exchanges need it. Fits
  the message variables' `{NAME}` syntax (see `docs/functional-spec.md`),
  as a built-in variable.
