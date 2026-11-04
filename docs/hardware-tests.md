# Pending Hardware Tests

Checks that need a person at the board (touchscreen, paddle, or a scope/LED
on the key output) and haven't been done yet. Checks that can be run over
the serial CLI are listed here too until someone has run them.

How this file works:

- Add an item when a change lands that can't be fully verified over serial.
- When an item passes on the board, **delete it** (don't tick it). If it
  fails, replace it with a note of what went wrong, or fix it and delete it.
- When a section is empty, delete the section.
- Unattended serial testing mutes the sidetone (`set volume_pct 0`) and
  restores it to 10% when done; `set` saves to flash, so if a session was
  cut short the board may still be muted. Several checks below are about
  what you hear, so make sure the volume is up first.
- Settings changes are saved about 2 s after the last tap. Wait a few
  seconds before rebooting to check that something was kept.

## Setup

- Use spare memory slots so real memories aren't overwritten, and note the
  current tile assignments first (`mem tiles`) to restore them afterwards.
- At least 8 non-empty test memories (for the queue check), with tiles 1–6
  assigned to them, the first long enough to keep playing for a while,
  e.g. from the CLI:
  `mem set 21 text "VVV VVV VVV VVV VVV VVV VVV VVV VVV VVV"`,
  `mem set 22 text "TEST"` … `mem set 28 text "TEST"`, then
  `mem tile 1 21` … `mem tile 6 26`. Afterwards, `mem clear 21` …
  `mem clear 28` and reassign the tiles.

## Message memories panel (`f36b0e8`, `a6f4be1`)

- [ ] Tap a tile: its memory plays; the tile turns amber with `▶`.
- [ ] Tap another tile while one plays: it queues, showing its queue
      position (`1`, `2`, …) in the corner. Tapping the playing or a queued
      tile does nothing.
- [ ] A tile assigned to an empty memory (e.g. `mem tile 6 30` with
      memory 30 empty) shows `#30 (empty)` and a tap does nothing.
- [ ] Long-press a tile: the memory picker opens (None row, 32 memories,
      current one highlighted, Cancel). Picking a row reassigns the tile and
      survives a reboot. Long-press is ignored on the playing/queued tile.
- [ ] Repeat tile: greyed until a tile has played; tap toggles
      `Repeat: ON/OFF` for the current tile with the behavior in the spec.
- [ ] Repeat tile long-press: popup with Count (0 = "No limit") and Pause
      `-`/`+`, hold to repeat, "Memory defaults", Close; changes apply at
      once to the playing message.
- [ ] Shorten the Pause in the popup while a repeat pause is running past
      its new end: the next send starts at once and is actually keyed.
- [ ] Stop tile stops playback and clears the queue.
- [ ] Sending strip: text scrolls with the current character highlighted
      amber; progress `2/5`, loop icon for count 0, seconds countdown during
      a pause; empty when idle.
- [ ] `Queue full` notice: with memory 21 playing, queue six others from
      the CLI (`mem play 23` … `mem play 28`), then tap tile 2: the strip
      shows `Queue full` for 2 s. (Tiles alone can't fill the queue: six
      tiles give at most one playing plus five queued.)
- [ ] Back returns to the Send screen with playback continuing; the sent
      text appears decoded there.
- [ ] Touching a paddle or the key during playback cancels it at once, the
      touch isn't keyed, and input resumes once both paddles are released.
- [ ] Tap a tile right after keying something (paddles released): the
      message starts after a clean word gap, not merged with your last
      element or character.

## Sidetone sharing (`91c1ad8`, `d4bda65`)

- [ ] While a message plays, the Settings > Sidetone "Test" button and
      the Morse Reference characters do nothing.
- [ ] Morse Reference playing a character when a message starts (CLI
      `mem play`): no stuck tone, and the message is heard cleanly from its
      next element.
- [ ] Help's Back while a message plays doesn't cut the message's tone.

## On-air / off-air (`7ac7e25`, `beb599a`)

- [ ] Send screen bottom row fits on screen with the new TX button and the
      icon-only Clear (trash) button; Clear still empties the text.
- [ ] TX toggles and turns red while on-air.
- [ ] Main Menu has three buttons that fit; the third reads `Off Air` /
      `On Air` and turns red while on-air.
- [ ] Settings > On Air switch shows the live state (also after using TX
      or the Main Menu button).
- [ ] Red frame shows on every screen while on-air, including Memories,
      Settings, Help and the Reset to Defaults popup; taps right at the
      screen edge still reach the buttons under it.
- [ ] A TX / Main Menu toggle is lost on reboot; a Settings > On Air change
      is kept (wait a few seconds after the change before rebooting).
- [ ] Scope or LED on the key output (GPIO17): off-air it never keys
      (paddle, straight key, message playback). Going on-air while holding a
      dah doesn't key the rest of it; the next element keys normally.

## Quick speed change (`2421fe0`)

- [ ] Long-press the WPM/mode label: the speed bar appears, and releasing
      does *not* open the Keyer submenu.
- [ ] A short tap on the label still opens the Keyer submenu.
- [ ] Holding `+`/`-` steps steadily, with no extra step on release; the
      label underneath updates.
- [ ] The bar hides after 5 s idle, on ✕, and when leaving the screen; it
      doesn't cover anything important.
- [ ] A WPM changed here survives a reboot (wait a few seconds after the
      last step before rebooting).
