# template-project

A starting point for ESP-IDF (v6.1) projects, built with the official
Espressif build system and testing conventions.

- Default target chip: **ESP32** (set via `sdkconfig.defaults`, `CONFIG_IDF_TARGET`).
  Change it at any time with `idf.py set-target <chip>`.
- ESP-IDF is expected to be managed via the ESP-IDF Installation Manager
  (EIM). Commands below assume `idf.py` is available (either through the EIM
  shell activation, or via `eim run "idf.py <command>" <VERSION>`).

## Layout

```
.
├── CMakeLists.txt          # top-level project definition
├── sdkconfig.defaults       # default target chip and app-wide config
├── main/                    # application entry point (app_main)
└── components/
    └── example_component/   # example hardware-independent component
        ├── include/
        ├── example_component.c
        └── host_test/
            └── example_component_test/   # native (Linux-target) Unity tests
```

`example_component` is a small debounce filter with no ESP-IDF hardware
dependencies, included purely to demonstrate the intended pattern: pure
logic lives in a component, and that component ships its own native test
project under `host_test/`.

## Building and flashing the application

```bash
idf.py build
idf.py -p <PORT> flash monitor
```

## Running unit tests natively (no device required)

Every component that has testable logic gets a `host_test/<component>_test`
subdirectory: a self-contained ESP-IDF project that targets Linux
(`CONFIG_IDF_TARGET_LINUX`) instead of a chip, links in only the component
under test plus Unity, and runs as a native executable. This is the official
ESP-IDF pattern for host-based testing (see `idf.py --preview set-target
linux` / `api-guides/host-apps` in the ESP-IDF docs), and it's how ESP-IDF
tests its own components (e.g. `components/nvs_flash/host_test`).

To build and run the tests for `example_component`:

```bash
cd components/example_component/host_test/example_component_test
idf.py build
./build/example_component_test.elf
```

A passing run ends with a Unity summary line such as `5 Tests 0 Failures 0
Ignored OK`.

### Host build prerequisites

Building for the Linux target compiles and links with your system's native
toolchain rather than a cross-compiler, so it needs:

- **`libbsd-dev`** (per ESP-IDF's own `host-apps` guide requirements):
  `sudo apt-get install libbsd-dev` on Debian/Ubuntu (see the guide for other
  distros/macOS).
- Your system `/usr/bin` (native `gcc`/`as`) resolved ahead of any ESP-IDF
  cross-toolchain directories (Xtensa/RISC-V/ULP `-elf-` toolchains) on
  `PATH`. If your shell activates the full EIM/ESP-IDF environment globally,
  those toolchain `bin/` dirs also ship their own generically-named `as`,
  which can shadow the system assembler and break the native build with
  errors like `as: unrecognized option '--64'`. If that happens, build with a
  trimmed `PATH` that puts `/usr/bin` first and only adds ESP-IDF's
  `cmake`/`ninja`/Python venv dirs (not the chip toolchains) — they aren't
  needed for a Linux-target build anyway.

### Adding tests for a new component

1. Create `components/<name>/host_test/<name>_test/`.
2. Copy the structure from `components/example_component/host_test/example_component_test/`:
   - `CMakeLists.txt` — sets `COMPONENTS main`, adds `EXTRA_COMPONENT_DIRS`
     pointing back at `components/`, then declares the project.
   - `sdkconfig.defaults` — pins `CONFIG_IDF_TARGET="linux"` and enables
     `CONFIG_UNITY_ENABLE_FIXTURE=y` (see below for why).
   - `main/CMakeLists.txt` — registers the test main component with
     `REQUIRES unity <name>`.
   - `main/test_runner.c` — the `app_main()` that drives Unity via
     `UNITY_MAIN_FUNC(run_all_tests)`, where `run_all_tests()` calls
     `RUN_TEST_GROUP(<name>)` for each test group.
   - `main/test_*.c` — test cases using the Unity **fixture** API:
     `TEST_GROUP(<name>)`, `TEST_SETUP`/`TEST_TEAR_DOWN`,
     `TEST(<name>, case_name) { ... }`, and a `TEST_GROUP_RUNNER(<name>)`
     that lists each case with `RUN_TEST_CASE`.
3. Keep the component's own logic free of hardware/driver dependencies where
   possible so it can run on the Linux target without mocks. Components that
   must depend on hardware APIs can still be host-tested using ESP-IDF's
   CMock-based mocking support, at the cost of extra setup.

#### Why the fixture API (`TEST_GROUP`/`TEST`), not the plain `TEST_CASE` macro

ESP-IDF's docs describe a simpler style — `TEST_CASE("desc", "[tag]") { ... }`
with automatic registration — for the `test` subdirectory of on-target unit
test apps. That macro registers each test via an `__attribute__((constructor))`
function in the same translation unit, with no other code referencing it.
That's fine on-target, but a standalone `host_test` project links component
sources as static libraries *without* `--whole-archive`, so the linker never
has a reason to pull a `.o` file out of the archive if nothing external
references a symbol in it — the constructor (and the test) silently vanishes,
and the run reports **0 Tests** with no error.

The fixture API avoids this because `main/test_runner.c` calls
`RUN_TEST_GROUP(<name>)`, which is a real, direct reference to a function
defined in `main/test_*.c` (`TEST_GROUP_RUNNER`) — that reference is what
forces the linker to include the test file's object code. `TEST_GROUP_RUNNER`
in turn calls each test case directly via `RUN_TEST_CASE`. This is the same
pattern ESP-IDF uses in its own `host_test` projects, e.g.
`components/esp_partition/host_test/partition_bdl_test`.
