#!/bin/sh
# Downloads Tajawal (SIL OFL) from the google/fonts repository into fonts/.
set -e
cd "$(dirname "$0")/.."
mkdir -p fonts
for f in Tajawal-Medium Tajawal-Bold; do
  [ -s "fonts/$f.ttf" ] || curl -fsSL -o "fonts/$f.ttf" \
    "https://raw.githubusercontent.com/google/fonts/main/ofl/tajawal/$f.ttf"
done
ls -l fonts
