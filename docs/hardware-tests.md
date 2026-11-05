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

## Sidetone sharing (`91c1ad8`, `d4bda65`)

- [ ] Morse Reference playing a character when a message starts (CLI
      `mem play`): no stuck tone, and the message is heard cleanly from its
      next element.

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

## CLI `send` (`0b0b563`, `7ff61af`)

- [ ] While a `send` message plays (e.g. `send "VVV VVV VVV VVV VVV"`),
      touching a paddle or the key cancels it at once and the touch isn't
      keyed.
- [ ] While a `send` or CLI `mem play` message plays, the Memories panel's
      Repeat tile is greyed out and does nothing, the sending strip shows
      the text, and no tile is highlighted as playing.

## Weight (`fc6bfac`, `a4a26f9`, and "Add the Weight setting…")

- [ ] Settings > Keyer is a 3×3 grid that fits and reads well: WPM, Key
      Mode, Paddle Swap; Paddle Debounce, Auto Space, Weight; two empty
      cells and Back. Back still returns to the right screen (Settings, or
      the Send screen when entered from its WPM label).
- [ ] The Weight popup steps 25–75% by 1 (hold to repeat); its tile stays
      enabled in Straight Key mode (Auto Space's is greyed out).
- [ ] By ear (sidetone up): weight 75% sounds heavier and 25% lighter than
      50% at the same WPM, with paddle keying; and the Send screen still
      decodes paddle keying correctly at both 25% and 75%.

## Bug mode

- [ ] Settings > Keyer > Key Mode shows five modes plus Close; picking Bug
      shows `Bug` on the Keyer tile and the Send screen's mode label,
      greys out Auto Space, and shows Paddle Debounce forced on.
- [ ] With a paddle in Bug mode: holding dit sends automatic dits at the
      set WPM; dah keys for exactly as long as it's held; a dah pressed
      during a dit starts after that dit; the Send screen decodes
      bug-style sending (e.g. hand-timed dahs of about 3 dits).
- [ ] Paddle Swap swaps which lever is the automatic one.

## Message variables

Set up from the CLI: `var set MYCALL DL1ABC`, `mem set 21 text "DE {MYCALL} {QTH}"`,
`mem tile 1 21`. Afterwards `mem clear 21`, `var clear MYCALL`,
`var clear QTH` and reassign tile 1.

- [ ] With `QTH` not set, tapping tile 1 sends nothing and the sending
      strip shows `{QTH} not set` for 2 s.
- [ ] After `var set QTH BERLIN`, tapping tile 1 sends `DE DL1ABC BERLIN`,
      and the sending strip shows that filled-in text with the highlight
      on the right characters.
- [ ] The tile and the memory picker show the memory's text as stored
      (`DE {MYCALL} {QTH}`), braces readable.
- [ ] Only if you're doing a Settings > Reset to Defaults anyway (it also
      erases the calibration and memories): afterwards `var list` is
      empty.
