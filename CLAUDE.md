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
- Currently installed/selected version: **v6.1**
  (`/home/dorin/.espressif/v6.1/esp-idf`).

## Standard Commands

Run via the MCP tools or `eim run "idf.py <command>"`:

- Select target: `set-target <TARGET>`
- Build: `build`
- Flash: `-p <PORT> flash`
- Monitor: `-p <PORT> monitor`
- Flash and monitor: `-p <PORT> flash monitor`
- Clean: `clean` / `fullclean`

`set-target` clears the build directory and moves the previous `sdkconfig` to
`sdkconfig.old`. Ask the user before running it — it replaces configuration
and build state. Do not run a redundant `fullclean` afterward; use it only
when stale artifacts are a demonstrated problem.

## Documentation

- Prefer the Espressif Documentation MCP server for ESP-IDF API and guide
  lookups over training-data memory, especially for version-sensitive APIs.
