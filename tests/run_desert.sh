#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/.."
python3 tools/gen_scales.py --check
mkdir -p build/assessment
case "$(uname -s)" in Darwin) set -- '-Dsection(x)=unused';; *) set --;; esac
cc "$@" -O1 -g -Wall -Wno-unused-function -fsanitize=address,undefined -fno-sanitize=shift \
    -fno-sanitize-recover=undefined -fno-omit-frame-pointer '-DAPP_VER="0.1-desert"' \
    -Ifirmware/src -Ifirmware/gen tests/desert_test.c -lm -o build/assessment/desert-test
build/assessment/desert-test
