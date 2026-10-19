#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/screens docs/screens/dakhmeh
case "$(uname -s)" in Darwin) set -- '-Dsection(x)=unused';; *) set --;; esac
cc "$@" -O2 -Wno-unused-function '-DAPP_VER="0.2-dev"' -Ifirmware/src -Ifirmware/gen -o build/screens/capture tools/capture_screens.c
build/screens/capture
python3 - <<'PY'
from pathlib import Path
from PIL import Image
names=['splash','desert-oud','maqam-rast','chords','tashriq','tuning','sequencer']
for name in names:
    Image.open(f'build/screens/{name}.ppm').resize((480,480),Image.Resampling.NEAREST).save(f'docs/screens/dakhmeh/{name}.png')
strip=Image.new('RGB',(4*480+5*16,512),(11,11,10))
for i,name in enumerate(['splash','desert-oud','maqam-rast','tashriq']):
    strip.paste(Image.open(f'docs/screens/dakhmeh/{name}.png'),(16+i*496,16))
strip.save('docs/screens/dakhmeh/overview.png')
print('Captured seven firmware screens and README overview')
PY
