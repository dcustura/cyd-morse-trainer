# Architecture & Mental Model

This is the "start here" document for anyone contributing to this codebase.
It explains how the pieces fit together and why they're organized the way
they are. It intentionally does not restate *what the device does* — see
[`functional-spec.md`](functional-spec.md) for the full screen-by-screen,
setting-by-setting behavior spec — this document is about *how the code
implementing that behavior is structured*.

## What this project is, in one paragraph

A Morse code **send trainer** on an ESP32 ("Cheap Yellow Display" board): a
FreeRTOS task samples a paddle/straight-key GPIO every millisecond, runs it
through a keyer state machine and a Morse timing decoder, and the resulting
characters are shown live on an LVGL touchscreen UI, logged, optionally typed
into a paired phone/PC over BLE HID, and readable/settable over a serial CLI.
Settings and touch calibration persist to NVS flash.

## The central design split: `components/` vs `main/`

This is the single most important thing to understand before touching code.

- **`components/*`** hold pure, hardware-independent logic: state machines and
  math with no ESP-IDF driver calls, no LVGL, no FreeRTOS primitives. Every
  one of them takes time as an explicit parameter (`now_ms`) rather than
  reading a clock itself, which is what makes them:
  - Unit-testable natively on Linux, with no chip and no mocks, under
    `components/<name>/host_test/<name>_test/` (see "Testing" below).
  - Reusable by both real hardware timing and CLI/test-driven synthetic
    timing.
- **`main/*`** is everything hardware-facing and stateful-at-the-app-level:
  GPIO configuration, the DAC-driven sidetone synthesizer, the LVGL screens,
  NVS persistence, BLE HID, and the serial CLI. It *drives* the components in
  `components/` but contains none of the decoding/keying logic itself.

When deciding where a change belongs: if it's "what does this sequence of
dits and dahs mean" or "what should the keyer do this tick given paddle
state," it belongs in a `components/` state machine and should get a host
test. If it's "how do we show that on screen" or "which GPIO is that wired
to," it belongs in `main/`.

## The five components

| Component | Responsibility |
|---|---|
| `debouncer` | Generic consecutive-sample debounce filter (used for paddle/key contacts). |
| `iambic_keyer` | Paddle-to-element state machine: Iambic Mode A/B, Ultimatic, and straight-key passthrough. Given raw dit/dah contact state and a timestamp, decides when dits/dahs start and stop. Also owns Auto Space: in Iambic/Ultimatic, `iambic_keyer_set_auto_space_pct()` makes it withhold starting a new element after too short a pause, so the signal it generates is itself correctly spaced rather than relying on lenient decoding. |
| `morse_codec` | Timing classifier + decode tree: given key-down/key-up transitions and timestamps, classifies dit vs. dah and gap vs. character-gap vs. word-gap, and turns finished element sequences into characters or prosigns. |
| `settings` | The `settings_t` struct, its field bounds/defaults, and pure clamp/validate helpers — no I/O. |
| `touch_calibration` | 5-point touch calibration math (compute from raw taps, apply to map a raw sample to a screen coordinate) — no I/O, no display driver calls. |

None of these know about each other, GPIOs, LVGL, or NVS. `main/` wires them
together.

## Data flow: a keystroke to the screen

This is the core runtime path, all running inside `paddle_input.c`'s
`paddle_task` (a dedicated FreeRTOS task polling at 1ms, `PADDLE_TICK_MS`):

```
GPIO (dit/dah, active-low)
   -> debouncer_feed()             [components/debouncer]      (optional per Paddle Debounce setting; always on for Straight Key)
   -> iambic_keyer_service()       [components/iambic_keyer]   -> key_down bool for this tick
   -> on key_down transition:
        sidetone_key(key_down)                    [main/sidetone.c]     -> audible tone, DAC
        gpio_set_level(KEY_OUTPUT_GPIO, ...)       [main/paddle_input.c] -> external CW key output
        morse_codec_key_event()    [components/morse_codec]    -> classifies the just-finished element/gap
   -> every 30ms while idle: morse_codec_tick()    [components/morse_codec] -> flushes a trailing char/word gap
   -> on a decode event (char / space / unknown):
        xQueueSend(decoded_char_queue, ...)        -> drained by an lv_timer in ui_practice.c, appended to the text area
        ble_hid_output_notify_char()               [main/ble_hid_output.c] -> optional BLE HID keystroke
        ESP_LOGI(...)                              -> serial log
```

The queue hand-off (`decoded_char_queue`, created in `main.c`) is the only
coupling between the real-time paddle task and the LVGL UI task — it's what
lets the 1ms-tick keying loop stay decoupled from LVGL's rendering/lock
timing.

## Concurrency model

Several independent FreeRTOS tasks/contexts, deliberately kept loosely
coupled:

- **`paddle_task`** (`main/paddle_input.c`): the 1ms real-time loop described
  above. Talks to the rest of the system only by writing to
  `decoded_char_queue`, calling `ble_hid_output_notify_char()` (itself
  non-blocking — queues to a dedicated BLE task), and an atomic
  `s_key_down_state` flag that the UI polls for the keying indicator dot.
- **LVGL's own task** (via `esp_lvgl_port`): owns all screen objects. Any code
  touching LVGL objects from another context must hold the LVGL lock
  (`lvgl_port_lock(0)` / `lvgl_port_unlock()`), as `main.c` does while
  building the initial screen tree. The `ui_practice.c` decode-queue drain
  runs as an `lv_timer`, so it's already on the LVGL task and needs no
  separate locking.
- **Sidetone's audio task** (`main/sidetone.c`): feeds the DAC continuous-mode
  driver; `sidetone_key()`/`sidetone_set_*()` are just parameter handoffs to
  it.
- **NimBLE's host task** (BLE HID, `main/ble_hid_output.c`): standard NimBLE
  host task; this module's own queued sender task decouples HID report
  sending from the paddle task's real-time deadline.
- **The CLI task** (`main/cli.c`): blocks on UART line input; `set` commands
  call the same live-apply setters the UI uses (`paddle_input_set_*`,
  `ui_settings_sync()` to keep tiles from going stale, `settings_store_save`).

Boot order in `main.c` is deliberate and commented in-line: display before
paddle input (paddle input's sidetone claims a DMA buffer that competes with
BLE for scarce internal DMA RAM, so audio must init first), and
`ble_hid_output_init()` after `paddle_input_start()` for that reason. Read the
comment in `app_main()` before reordering anything there.

## Persistence

Two independent NVS-backed stores, both following the same pattern
(`*_store_init_and_load` / `*_store_save` / `*_store_reset`, wrapping a
hardware-independent value type from `components/`):

- **`main/settings_store.c`** persists `settings_t` (keyer/sidetone/display/
  Bluetooth settings). Missing or out-of-range stored values fall back to
  `components/settings`'s compiled-in defaults/clamps field-by-field, so a
  partially-corrupt or older-schema NVS blob degrades gracefully instead of
  failing to boot.
- **`main/touch_cal_store.c`** persists a `touch_calibration_t`. Its presence
  or absence is also the boot-flow branch: no stored calibration means the
  device boots straight into the Touch Calibration screen instead of the Main
  Menu (see `main.c`, `touch_cal_found`).

`settings_store` debounces writes by a couple of seconds after the last
edit (inside `ui_settings.c`, not the store itself) to reduce flash wear
from rapid-fire `-`/`+` taps. `touch_cal_store` has no such debounce — it
saves synchronously, once, right after the 5th calibration tap
(`ui_calibration.c`'s `poll_timer_cb()`), which is fine since calibration
is a rare, one-shot action rather than something tapped repeatedly.

## The UI layer (`main/ui_*.c`)

Built on LVGL 9, with each screen a standalone `.c`/`.h` pair created once at
boot in `main.c` and switched between via `lv_scr_load()`:

- `ui_menu.c` — Main Menu (two buttons, Practice/Settings).
- `ui_practice.c` — Practice screen: drains `decoded_char_queue`, owns the
  WPM/Mode status label and the keying indicator dot.
- `ui_settings.c` (the largest file, ~850 lines) — the entire Settings screen
  and all its submenus/popups (Keyer, Sidetone, Display, Touchscreen,
  Bluetooth). One file because the submenus share a lot of popup-building and
  tile-styling helpers; see its header comment for the full navigation map.
- `ui_calibration.c` — the 5-point touch calibration flow, driving
  `components/touch_calibration`.
- `ui_touch_test.c` — the Verify Calibration draw-a-dot screen.
- `ui_help.c` — the Morse Reference screen (tabbed character/prosign charts).

Cross-screen navigation is wired with plain `lv_obj_t*` pointers passed
around at creation time (e.g. `ui_practice_create()` takes `menu_screen`, and
`ui_settings_set_keyer_back_target()` lets the Practice screen's WPM/Mode
shortcut retarget the Keyer submenu's Back button at navigation time). There
is no generic navigation stack — each screen's Back target is either fixed at
creation or explicitly retargeted right before navigating to it.

`main/display_init.c` owns the SPI/panel/touch bring-up and a handful of
shared styling helpers (`display_style_tile`, `display_style_button_dismiss`,
`display_compensate_color`, etc.) used across every `ui_*.c` file so tiles,
buttons, and colors stay visually consistent without duplicating LVGL style
boilerplate per screen. It also documents this board's two hardware
surprises found during bring-up (ST7789 controller despite ILI9341-labeled
community docs; swapped/partial-range/IRQ-unreliable touch axes) — read its
top comment and `board_pins.h` before debugging anything display- or
touch-related.

## Hardware configuration

All GPIO assignments are Kconfig options (`main/Kconfig.projbuild`, menu
"Morse Trainer Hardware"), not hardcoded — `main/board_pins.h` just aliases
`CONFIG_MORSE_*` to shorter `BOARD_*` names. This exists because clone boards
of the ESP32-2432S028R vary across production batches; if you're bringing up
a different unit, `idf.py menuconfig` under that menu is the first place to
look, and `board_pins.h`'s table comment records what's been confirmed
against real hardware versus assumed. Two pins (`MORSE_SPEAKER_GPIO`,
`MORSE_LDR_GPIO`) have a hardcoded channel/ADC-unit companion constant
elsewhere in code (`sidetone.c`, `auto_brightness.c`) that must be kept in
sync manually if you change them — noted in the Kconfig help text.

## Build system

- Standard ESP-IDF CMake project (`CMakeLists.txt` at the root just includes
  ESP-IDF's `project.cmake`).
- Target chip and app-wide config live in `sdkconfig.defaults` — read it
  before assuming a default; several entries there exist to work around a
  specific bug (e.g. `CONFIG_FREERTOS_HZ=1000` for genuine 1ms ticks,
  `CONFIG_LV_TXT_LINE_BREAK_LONG_LEN` variants so LVGL wraps long spaceless
  decoded-text runs) and the comments explain why.
- `partitions.csv` is a custom partition table — LVGL + `esp_lcd` + the touch
  driver leave little headroom under the default ~1MB factory app partition.
- External dependencies (`main/idf_component.yml`): LVGL 9, `esp_lvgl_port`,
  `esp_lcd_touch`, and the XPT2046 touch driver, pulled from the ESP
  Component Registry — see `dependencies.lock` for pinned resolved versions.
- BLE stack choice (NimBLE, not Bluedroid) and its trimmed role/connection
  config are explained inline in `sdkconfig.defaults` — driven by this
  board's tight flash/SRAM budget and a real measured conflict between BLE's
  and the sidetone DAC's DMA memory needs.

## Testing

Every `components/*` module has a native (Linux-target) Unity test project
under `components/<name>/host_test/<name>_test/`, run with plain `idf.py
build && ./build/<name>_test.elf` — no chip required. See the README's
"Running unit tests natively" and "Adding tests for a new component" sections
for the exact structure to copy and *why* it uses Unity's fixture API
(`TEST_GROUP`/`TEST`) rather than the simpler `TEST_CASE` macro (the short
version: `TEST_CASE`'s constructor-based auto-registration gets silently
linked out of a standalone host test binary since nothing else references
it).

`main/*` has no unit tests — it's hardware/LVGL glue, which is why the
`components/` vs `main/` split above matters: pushing logic into a component
is what makes it testable at all.

## Where to look first

| I want to change... | Start in |
|---|---|
| How dits/dahs decode into characters, prosign handling, timing thresholds | `components/morse_codec/` |
| Iambic Mode A/B/Ultimatic keying behavior, squeeze/latch logic | `components/iambic_keyer/` |
| A new setting (field, range, default) | `components/settings/include/settings.h` first, then `main/settings_store.c` (persistence) and `main/ui_settings.c` (UI) and `main/cli.c` (CLI key) |
| Practice screen appearance/behavior | `main/ui_practice.c` |
| Settings screen tiles/popups/navigation | `main/ui_settings.c` |
| Touch calibration math | `components/touch_calibration/` |
| Touch calibration *screens* | `main/ui_calibration.c`, `main/ui_touch_test.c` |
| Sidetone pitch/envelope/synthesis | `main/sidetone.c` |
| BLE HID keystroke mapping | `main/ble_hid_output.c` |
| Serial CLI commands | `main/cli.c` |
| GPIO pin assignments | `main/Kconfig.projbuild` (+ `main/board_pins.h`) |
| Boot sequence / subsystem init order | `main/main.c` (read its trailing comment before reordering) |

## Further reading

- [`functional-spec.md`](functional-spec.md) — the authoritative behavior
  spec: every screen, setting, default, and edge case. Keep it in sync with
  any functionality change (see `AGENTS.md`).
- [`../README.md`](../README.md) — hardware summary, build/flash commands,
  and the full native-testing workflow.
- [`../AGENTS.md`](../AGENTS.md) — repo-wide conventions (documentation
  upkeep, UI grid layout rules) that apply to any change, human or agent.
