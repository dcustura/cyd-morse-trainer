# ESP-IDF Agent Instructions

This project is built with ESP-IDF, managed via the ESP-IDF Installation
Manager (EIM). Follow these baseline rules when working here.

## Discover the Project First

- Locate the ESP-IDF project root before running commands. A project root
  normally contains a top-level `CMakeLists.txt` with a `project(...)` call.
- Read the top-level `CMakeLists.txt`, `sdkconfig.defaults*`, component
  manifests, and partition table before choosing commands.
- Determine the required ESP-IDF version and target chip from
  `sdkconfig.defaults` (`CONFIG_IDF_TARGET`) rather than assuming one.

## Environment

- ESP-IDF is managed by EIM, not a manually sourced `export.sh`. Do not rely
  on `idf.py` being a plain command on `PATH` — activation defines it as a
  **shell function/alias**, which does not propagate into subshells you spawn
  (e.g. via a Bash tool). Prefer one of:
  - The **ESP-IDF Tools MCP server** (`esp-idf-eim` — `set_target`,
    `build_project`, `flash_project`, `clean_project`, `create_project`, and
    the `project://status`, `project://config`, `project://devices`
    resources) when available. This is the recommended integration and needs
    no shell activation.
  - `eim run "idf.py <command>" <VERSION>` for one-off commands — spawns a
    fresh process with the environment fully set up, independent of shell
    state.
- Useful EIM commands:
  - `eim list` — installed versions, install paths, and which one is selected
  - `eim select <VERSION>` — change the selected version
  - `eim run "idf.py --version" <VERSION>` — verify a specific environment
- Use the installation path reported by `eim list` when a task needs
  `IDF_PATH` or direct access to a matching ESP-IDF checkout. Do not guess
  paths.

## Standard Commands

`set-target` clears the build directory and moves the previous `sdkconfig` to
`sdkconfig.old`. Ask the user before running it — it replaces configuration
and build state. Do not run a redundant `fullclean` afterward; use it only
when stale artifacts are a demonstrated problem.

## Project Goals and Backlog

- This project is a **CW keyer + Morse send trainer**. It started as a pure
  send trainer and is being extended into a full-featured, easy-to-use
  keyer for on-air operation. Don't drop or weaken trainer behavior when
  adding keyer features.
- `docs/backlog.md` is the curated list of planned features that **aren't
  implemented yet**. `docs/functional-spec.md` describes only what's
  implemented.
  - Check the backlog before proposing or starting new feature work, and
    follow its priority tiers unless the user says otherwise.
  - When a backlog feature is implemented, in the same change: describe its
    final behavior in `docs/functional-spec.md`, and **remove its entry from
    `docs/backlog.md`** (don't mark it done). If only part of an entry is
    done, cut the entry down to what's left.
  - Keep the backlog curated: add newly agreed ideas, and update or drop
    entries when plans change or a constraint is discovered.

## Hardware Tests

- `docs/hardware-tests.md` lists checks that need a person at the board
  (touchscreen, paddle, scope on the key output) and haven't been done.
  When a change can't be fully verified over serial, add its checks there
  in the same change; add serial-CLI checks too if they weren't run. Delete
  an item once it has passed (the user confirms on-board items).

## Documentation

- Prefer the Espressif Documentation MCP server for ESP-IDF API and guide
  lookups over training-data memory, especially for version-sensitive APIs.
