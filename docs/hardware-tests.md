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

## Message variables

- [ ] Only if you're doing a Settings > Reset to Defaults anyway (it also
      erases the calibration and memories): with a variable set beforehand
      (e.g. `var set MYCALL DL1ABC`), `var list` is empty afterwards.
