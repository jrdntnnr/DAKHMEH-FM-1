# Building DAKHMEH

DAKHMEH is one C program, `firmware/src/grimm.c` (it includes the other `.c` files), on the FM-1 platform of
[Felucca](https://github.com/hugelton/Felucca) as [SLOOP](https://github.com/isod89/sloop-fm1) and
[Dinghy](https://github.com/zvenson/dinghy) carry it. The same file builds three ways: for the FM-1, for your
computer (host mode: the screen as images, the sound as a WAV) and for the browser (the demo and the editor).

## Set up once

Linux on x86-64 (or Windows with WSL, or macOS with Docker), Python 3 with Pillow, the JieLi toolchain and three
files of the JieLi AC79 SDK:

```sh
pip3 install Pillow mido python-rtmidi
tools/get_toolchain.sh                      # JieLi's clang for the chip, to ~/.jieli/toolchain
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
    https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
```

On macOS the build runs the toolchain in a `linux/amd64` Docker container by itself (have Docker running).
`JIELI_TOOLCHAIN` and `AC79_SDK` point elsewhere if your copies live somewhere else.

For the browser build: the [Emscripten SDK](https://emscripten.org) in `~/emsdk`, and Node.js for its tests.

## The firmware

The current port is not hardware-qualified. The package still uses the
inherited development identity `FM-1_966`; a distinct release identity and
updater checks remain pending. Do not use an older build as the final release.


```sh
./build.sh                                  # -> build/dakhmeh.fwsc (identity FM-1_966, version 0.2-dev)
./build.sh --release 0.2                    # -> build/dakhmeh-0.2.fwsc, the version "0.2" on the screen
py tools/fm1_install.py build/dakhmeh.fwsc    # install it over USB (asks first); --info: what the FM-1 runs
```

The build fails on what would brick or break an FM-1 firmware: a 64-bit division, a call into the chip ROM, too
much RAM, a register touched outside `firmware/hal/`. [RULES.md](RULES.md) says why each one matters.

## Host mode and the tests

```sh
tests/run_host.sh                           # -> build/host/grimm.wav and screen PNGs
tests/stress.sh 1                           # the stress test (seeds 1, 2, 3), with address and undefined-behaviour checks
```

## The browser: demo and editor

```sh
web/build_web.sh                            # -> web/dist/ (index.html: the demo, editor.html: the editor, grimm.wasm)
node web/test_web.mjs                       # after tests/run_host.sh: the browser build against host mode, the editor
cd web/dist && python3 -m http.server 8000  # then open http://localhost:8000/ (or /editor.html)
```

`APP_VER=0.2-dev web/build_web.sh` puts the version on the browser build's screen.

## The factory samples

`python3 tools/gen_samples.py` rebuilds the four desert sample banks from
`assets/desert/*.wav`. Source credits and output hashes are beside the WAVs.

## Screenshots and splash

```sh
sh tools/capture_screens.sh
python3 tools/gen_dakhmeh_wordmark.py  # optional: Pillow, fontTools[woff], hb-view
```

Screenshots use the actual firmware framebuffer, scaled 2× without smoothing.
Normal builds use the checked-in Persian wordmark; no shaping library is
needed on the device.

## More

- [API.md](API.md): everything `platform.c` gives the app: the panel's ids, the screen calls, the audio contract,
  MIDI, settings, the flash map, timing.
- [RULES.md](RULES.md): the chip's rules (no FPU, no libgcc, the audio interrupt's budget).
- [EDITOR_PROTOCOL.md](EDITOR_PROTOCOL.md): how the editor talks to the FM-1 over USB SysEx.

## macOS account lookup failure

If `limactl` reports `user: unknown userid 501` and native SSH reports
`No user exists for uid 501`, the calling session cannot resolve the macOS
account. This does not establish a guest VM failure.

For an already running Lima VM with an active SSH master:

```sh
python3 tools/lima_shell.py --socket "$LIMA_HOME/ghoul/ssh.sock" -- /usr/local/bin/nerdctl info
```

The helper uses OpenSSH's documented connection-sharing protocol and passes
stdin/stdout/stderr plus the remote exit status. It preserves literal arguments
and supports concurrent builds. It reads no SSH keys and starts no VM.

The local Docker wrapper uses normal `limactl shell` when account lookup works,
and this helper when it fails. Its original version is kept beside it as
`docker.limactl-original`. If the VM/master is stopped, start Lima from a
normal macOS terminal first; the helper cannot create a new SSH connection.

Verified on the current VM: four concurrent sessions, literal argument quoting,
stderr and exit-status propagation, the full target build, and the target-only
duplicate-symbol check. This repairs build access; it does not alter macOS
account records or the guest disk.
