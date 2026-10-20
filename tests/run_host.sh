#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# GR!MM changes (file names): Copyright (C) 2026 GR!MM
# Run firmware/src/grimm.c on this computer (tests/host.c): build/host/grimm.wav and the screens as PNG.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host
case "$(uname -s)" in Darwin) set -- '-Dsection(x)=unused';; *) set --;; esac
cc "$@" -O2 -Wall -Wno-unused-function -DDINGHY_HOST -DAPP_VER="\"${APP_VER:-$(cat VERSION)}\"" -Ifirmware/src -Ifirmware/gen -o build/host/grimm firmware/src/grimm.c
build/host/grimm build/host/grimm
python3 - <<'PY'
from pathlib import Path
from PIL import Image
for p in sorted(Path("build/host").glob("grimm-*.ppm")):
    Image.open(p).resize((480, 480), Image.NEAREST).save(p.with_suffix(".png"))
    p.unlink()
    print("screen ->", p.with_suffix(".png"))
PY
cc -O1 -w -o build/host/enc_test tests/enc_test.c && build/host/enc_test | tail -1   # the knobs' detent decoder (hal/fm1_input.h)
cc "$@" -O2 -w -DAPP_VER="\"test\"" -Ifirmware/src -Ifirmware/gen tests/clean_test.c -lm -o build/host/clean_test && build/host/clean_test | tail -1   # no clicks: every knob under a chord
cc "$@" -O2 -w -DAPP_VER="\"test\"" -Ifirmware/src -Ifirmware/gen tests/split_test.c -lm -lpthread -o build/host/split_test   # the second core's share
build/host/split_test | tail -1                                      # (the job inline at the wait)
CORES=2 build/host/split_test | tail -1                              # (a thread)
CORES=2 CHAOS=5 build/host/split_test | tail -1                      # (a slow thread)
CPU1_TIMEOUT_AT=40 build/host/split_test | tail -1                   # (a job lost: the fallback)
