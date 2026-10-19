# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 GR!MM
"""The stress test's device check (tests/stress.sh): on the FM-1 grimm.c, platform.c, usb.c and the rest are one file,
and C quietly makes two file-scope variables of the same name and type one variable (GR!MM's key-echo ring and usb.c's
MIDI-out ring once shared mo_w / mo_r: the main loop chased itself, the watchdog restarted the FM-1). Host mode has no
usb.c, so only the device build shows it: the toolchain's preprocessor makes the device's file, gcc lists every name
declared twice (-Wredundant-decls), and a variable among them fails (a function's prototype, an extern: fine).

    python3 tests/device_names.py        (prints nothing; exit 1 and the names on a clash)"""
import os
import re
import subprocess
import sys
import shutil
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools"))
import build

tc = os.environ.get("JIELI_TOOLCHAIN", os.path.expanduser("~/.jieli/toolchain"))
clang = os.path.join(tc, "pi32v2", "bin", "clang")
if not os.path.exists(clang):
    print("device names: no toolchain (" + clang + "): not checked")
    sys.exit(1)
os.environ.setdefault("JIELI_TOOLCHAIN", tc)
os.makedirs("build/stress", exist_ok=True)
pre = "build/stress/device.i"
build.tc("cc", "-E", "-Os", "-fno-builtin", "-Ifirmware/hal", "-Ifirmware/src", "-Ifirmware/gen",
         '-DFELUCCA_ID="FM-1_966"', '-DAPP_VER="0.0"', "firmware/src/grimm.c", "-o", pre)
gcc = os.environ.get("HOST_GCC") or shutil.which("gcc-14") or "gcc"
probe = subprocess.run([gcc, "-fsyntax-only", "-Wredundant-decls", "-x", "c", "-"],
                       input="int check_duplicate; int check_duplicate;\n", capture_output=True, text=True)
if probe.returncode or "redundant redeclaration" not in probe.stderr:
    print("device names: HOST_GCC must provide GCC's redundant-declaration diagnostics")
    sys.exit(1)
r = subprocess.run([gcc, "-fsyntax-only", "-m32", "-Wredundant-decls", "-x", "c", pre],
                   capture_output=True, text=True)
if r.returncode:
    print(r.stderr)
    sys.exit(r.returncode)
out = r.stderr
bad = []
for m in re.finditer(r"^(\S+?):(\d+):(\d+): warning: redundant redeclaration of .(\w+).", out, re.M):
    path, line, col, name = m.group(1), int(m.group(2)), int(m.group(3)), m.group(4)
    try:
        text = open(path, encoding="utf-8", errors="replace").read().split("\n")[line - 1]
    except OSError:
        text = ""
    rest = text[col - 1 + len(name):].lstrip()
    if rest.startswith("(") or "extern" in text:
        continue                                # (a function declared ahead, an extern: one thing, as meant)
    bad.append(f"{path}:{line}: {name}")
if bad:
    print("device names: one variable under two declarations (the device's single file):")
    print("\n".join("  " + b for b in bad))
    sys.exit(1)
