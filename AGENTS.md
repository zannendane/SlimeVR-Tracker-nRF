# AGENTS.md

Zephyr RTOS firmware application for Nordic nRF52/nRF54L IMU trackers (SlimeNRF).
Not a host application — there is no JS/Python/Cargo build. It builds inside an
nRF Connect SDK (NCS) + Zephyr west workspace.

## Toolchain (setup is heavy and mandatory)

- `west.yml` pins `sdk-nrf` to revision `v3.1-branch`; CI uses Zephyr SDK `0.16.9`.
  These versions are coupled — don't bump one without the other.
- Requires a full west workspace: `west init -l <this-repo>` then `west update`,
  plus Zephyr SDK installed and `ZEPHYR_BASE` resolvable. A bare checkout cannot
  build on its own. (Locally this is normally via nRF Connect for Desktop's
  Toolchain Manager.)
- Python deps come from the SDK's `zephyr/scripts/requirements.txt`, not this repo.

## Build

CI's exact invocation (the canonical way to build):

```
west build --board <BOARD> --pristine=always <this-repo> --build-dir build \
  -- -DNCS_TOOLCHAIN_VERSION=NONE -DBOARD_ROOT=<path-to-this-repo>
```

- `-DBOARD_ROOT=.../SlimeVR-Tracker-nRF` is **required**: board definitions live
  in this repo under `boards/`, not in the SDK. Without it the custom boards
  won't resolve.
- **Sysbuild is required** (`sysbuild.cmake`, `sysbuild: true` in `sample.yaml`).
  It pulls in a bootloader image and uses static partition-manager config from
  `pm_static/` (matched by board/soc + `uf2` qualifier). Don't bypass sysbuild.
- Output artifact: `build/<app>/zephyr/zephyr.{hex,uf2}` (format depends on board;
  UF2 boards use the Adafruit bootloader).
- The canonical board targets are the matrix in `.github/workflows/workflow.yml`
  (e.g. `promicro_uf2/nrf52840/i2c`, `xiao_ble/nrf52840/sense`,
  `slimevrmini_p3r6_uf2/nrf52833`, `nrf52840dk/nrf52840`). When in doubt about a
  valid board target, read that matrix.

## Verification

- **CI only builds — there is no test, lint, format, or typecheck step.** Build
  success (for the matrix of boards) is the only automated check.
- `sample.yaml` declares a twister test config (`platform_allow` + `sysbuild`),
  but CI never invokes twister. Treat it as board-allow metadata, not a test suite.
- There is no clang-format / lint config; match surrounding style manually.

## Repo layout & architecture

- `src/main.c` — entrypoint: boot, reset-count / pairing / shutdown logic, then
  hands off to subsystems. Most work happens in dedicated threads.
- `src/sensor/` — IMU drivers (`imu/`), magnetometer drivers (`mag/`), fusion
  (`fusion/`, currently only VQF via `SENSOR_USE_VQF` Kconfig choice), I2C/SPI
  bus scanning (`scan*.c`), and calibration. `sensor.c` is the sensor thread.
- `src/connection/` — radio link via Nordic ESB (`esb.c`) and USB HID
  (`hid.c`, `usb.c`). Output goes over ESB, or HID when `CONNECTION_OVER_HID` is set.
- `src/system/` — battery, LED status, power/shutdown, status flags.
- `vqf-c/` — vendored VQF fusion library, **tracked in git (not a submodule)**.
  Edit only if fixing fusion math upstream.

## Build-system quirks (easy to miss)

- Source is globbed: `FILE(GLOB_RECURSE src/*.c)` and `vqf-c/src/*.c`. New `.c`
  files under `src/` are picked up automatically — no `CMakeLists.txt` edit
  needed. Include dirs are `src` and `vqf-c/src`.
- `VERSION` is **generated** from `VERSION_SRC` by CMake using `git describe`
  (major/minor/patch from tags, branch as extra version). `VERSION` is gitignored
  — never hand-edit it; edit `VERSION_SRC` only if changing the template. Builds
  outside a git checkout fall back to `0.0.0-0` / `unknown`.
- `src/build_defines.h` is force-touched on every build (`touch_util_h` target)
  to refresh the compile timestamp. Don't be surprised by it being "modified".
- Board variants are encoded as `_<qualifier>` files inside
  `boards/<vendor>/<board>/` (e.g. `promicro_uf2_i2c.dts` + `.yaml` +
  `_defconfig`), and the board target is `<board>/<soc>/<qualifier>`.
  SoC-level overlays live in `socs/` (e.g. `nrf52840_i2c.overlay`,
  `nrf52840_uf2.overlay`). Root-level `<board>.conf`/`.overlay` files also apply.
- Config stack: root `Kconfig` (app-specific options) → `prj.conf` (base) →
  per-board `.conf`. Runtime user settings persist in **retained memory**, mapped
  in `src/config.c` (see `config_settings_names[]`); defaults come from Kconfig.

## Conventions

- App source (`src/`, `vqf-c/`) is dual-licensed MIT/Apache-2.0 (see `README.md`).
  New `.c/.h` files should carry the SlimeVR MIT header used across `src/`.
  The Nordic `LicenseRef-Nordic-5-Clause` headers in `CMakeLists.txt`/`Kconfig`
  are from the original Nordic sample scaffolding — leave those files' headers as-is.
- `src/build_defines.h` mirrors SlimeVR-Server protocol enums (`SVR_BOARD_*`,
  `SVR_MCU_*`, `SVR_IMU_*`) and `get_server_constant_imu_id()`. When adding a new
  IMU or board, update both the `SVR_*` constants and the mapping functions there,
  keeping them in sync with
  [SlimeVR-Server `FirmwareConstants.kt`](https://github.com/SlimeVR/SlimeVR-Server/blob/main/server/core/src/main/java/dev/slimevr/tracking/trackers/udp/FirmwareConstants.kt).
- Logging is primarily over **SEGGER RTT** (`CONFIG_LOG_BACKEND_RTT=y`), with
  UART/CDC-ACM console also available. `CONFIG_LOG` is enabled; mind
  `CONFIG_LOG_BUFFER_SIZE` / stack sizes in `prj.conf` when adding verbose logs.
