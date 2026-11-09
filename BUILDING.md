# Building FMtribe

FMtribe is an app on [fm1-base](https://github.com/charlesvestal/fm1-base) (`base/`, a git submodule pinned at
`b0976f2`, with felucca-bsp inside it). Clone with `--recursive`, or after cloning:

```
git submodule update --init --recursive
```

`./build.sh` runs fm1-base's build (`base/tools/build.py`) on `app/`, whose settings are in `app/fm1.toml`. It
makes, in `app/build/`:

| File | What |
| --- | --- |
| `fmtribe.bin` | the firmware app |
| `loader/ota.bin` | the update loader (felucca-bsp's) |
| `fmtribe.fwsc` | the installable package (app + loader) |

The build also writes `firmware.json` at the repository's root, which firmware hubs read; commit it.

## Prerequisites

The host simulator needs only a C compiler and Python 3 with Pillow. The firmware build needs, as for GIO-1:

- Python 3 with Pillow: `pip3 install Pillow` (Homebrew's Python, with libraqm, for the fonts).
- Docker Desktop (or podman). The JieLi toolchain is Linux x86-64 only; on a Mac the build runs it in a
  `linux/amd64` container. Keep the source tree under `/Users`.
- The JieLi Linux toolchain at `~/.jieli/toolchain` (see fm1-base's README, `base/tools/get_toolchain.sh`).
- The JieLi AC79 SDK (Apache-2.0) at `~/fw-AC79_AIoT_SDK`:

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- Emscripten (optional, for the browser emulator) and Node.js.

`JIELI_TOOLCHAIN`, `AC79_SDK` and `PYTHON` override the defaults.

## Build

```
./build.sh                    # app/build/fmtribe.fwsc (with Bluetooth MIDI)
./build.sh --no-ble           # USB-only fallback build
./build.sh --release X.Y      # app/build/fmtribe-X.Y.fwsc, identity FM-1_59XXYYZZ
./build.sh --host             # the simulator: app/build/host/fmtribe_host
./build.sh --web              # the browser emulator: app/build/emu/
```

The build checks the image, RAM and pool sizes, and that no double-precision code is linked (FMtribe's DSP is
`float` only).

## Tests

```
sh tests/run_tests.sh         # FMtribe's
sh base/tests/run_tests.sh    # fm1-base's own
```

## Install

```
python3 base/tools/fm1_install.py app/build/fmtribe.fwsc
```

## Releases

Push a tag `vX.Y` (or `vX.Y-alpha` for a prerelease). `.github/workflows/release.yml` builds it on GitHub with the
shared FM-1 builder (`zvenson/fm1-build@v1`), runs `tests/run_tests.sh`, signs build provenance and publishes the
release. Notes come from `docs/releases/TAG.md` if present. The site:

```
gh release download vX.Y --pattern '*.fwsc' --dir app/build
base/tools/publish_site.sh app X.Y app/build/fmtribe-X.Y.fwsc
```

## Updating fm1-base

fm1-base is beta. Move the pin deliberately, one known-good commit at a time:

```
git -C base fetch origin && git -C base checkout <commit> && git submodule update --init --recursive
sh tests/run_tests.sh && git add base && git commit -m "base: fm1-base <commit>"
```
