# example_component host test

| Supported Targets | Linux |
| ------------------ | ----- |

Native unit test project for the `example_component` component. It builds and
runs directly on the host (no target device required), using the Unity test
framework via ESP-IDF's Linux target (`CONFIG_IDF_TARGET_LINUX`).

See the repo root `README.md` for host-build prerequisites (`libbsd-dev`,
`PATH` ordering) and why these tests use Unity's fixture API
(`TEST_GROUP`/`TEST`) instead of the plain `TEST_CASE` macro.

## Build

```bash
cd components/example_component/host_test/example_component_test
idf.py build
```

`sdkconfig.defaults` in this directory already pins `CONFIG_IDF_TARGET` to
`linux`, so a plain `idf.py build` is enough.

## Run

```bash
idf.py monitor
```

or run the built ELF directly:

```bash
./build/example_component_test.elf
```

A successful run prints a Unity summary ending in `OK` for every test case.
