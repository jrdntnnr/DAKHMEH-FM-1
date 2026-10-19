#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# After Felucca's web/make_site.py (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments), by way of
# SARDAB's; DAKHMEH changes.
"""Make DAKHMEH's GitHub Pages site from web/dist (web/build_web.sh) and a release package:

  index.html, panel.js, ...   the emulator (web/dist, as built: its version is VERSION's)
  editor.html, ...            the editor
  install.html                the installer (fm1pkg.js, fm1ota.js: Felucca's update protocol over WebMIDI)
  firmware/dakhmeh-VER.fwsc   the package, firmware/latest.json what the installer reads
  LICENSES/index.html         the licence texts listed (Pages lists no folders)

  python3 tools/make_site.py build/dakhmeh-0.3.fwsc build/site

The package must be a tools/build.py --release VER build of VERSION: its identity FM-1_9xx, Felucca's own loader in it
(never a package patched from an official one: no vendor files), every file's CRC good, VER in its app.
"""
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import fm1_install  # noqa: E402  (the same package checks the command-line installer makes)


def main(pkg, out):
    pkg, out = Path(pkg), Path(out)
    version = (ROOT / "VERSION").read_text().strip()
    dist = ROOT / "web" / "dist"
    raw = pkg.read_bytes()
    product, image = fm1_install.load_package(str(pkg), False)   # (identity, loader marker, header, files' CRCs)
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a Felucca package (FM-1_9xx)")
    app = ROOT / "build" / "grimm.bin"           # (the package holds it compressed: its own build's app is checked, the
    if not app.exists() or (version + "\0").encode() not in app.read_bytes() or \
            not 0 <= pkg.stat().st_mtime - app.stat().st_mtime < 600:   # package made from it, in the same run)
        raise SystemExit(f"{pkg}: not built as version {version} (VERSION); run tools/build.py --release {version}")
    wasm = (dist / "grimm.wasm").read_bytes() if (dist / "grimm.wasm").exists() else b""
    if (version + "\0").encode() not in wasm:
        raise SystemExit(f"web/dist: the emulator is not version {version}; run web/build_web.sh")
    if out.exists():
        shutil.rmtree(out)
    shutil.copytree(dist, out)
    for f in ("install.html", "fm1pkg.js", "fm1ota.js"):
        if not (out / f).exists():
            raise SystemExit(f"web/dist/{f} missing; run web/build_web.sh")
    fw = out / "firmware"
    fw.mkdir()
    name = f"dakhmeh-{version}.fwsc"
    shutil.copy(pkg, fw / name)
    (fw / "latest.json").write_text(json.dumps({
        "version": version, "product": product, "pkg": f"firmware/{name}", "size": len(raw),
        "sha256": hashlib.sha256(raw).hexdigest()}, indent=1) + "\n", encoding="utf-8")
    names = sorted(p.name for p in (out / "LICENSES").glob("*.txt"))
    (out / "LICENSES" / "index.html").write_text(
        '<!doctype html><meta charset="utf-8"><title>DAKHMEH licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a></p>\n',
        encoding="utf-8")
    (out / ".nojekyll").touch()
    print(f"site: {out}: emulator, editor, install.html, firmware/{name} ({len(raw)} B, {product}, {version})")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(*sys.argv[1:3])
