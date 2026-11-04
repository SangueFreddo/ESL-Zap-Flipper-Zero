#!/bin/sh
# ESL Zap host tests. Needs gcc, python3 + Pillow and a PrecIR checkout:
#   git clone https://github.com/furrtek/PrecIR /tmp/PrecIR
#   sh tests/run_tests.sh /tmp/PrecIR
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../src"
PRECIR=${1:-/tmp/PrecIR}
OUT=$(mktemp -d)
gcc -O1 -Wall -I "$HERE/host/stubs" -o "$OUT/ez_host" "$HERE/host/host_test.c" \
    "$SRC/proto/ez_proto.c" "$SRC/proto/ez_models.c" "$SRC/proto/ez_nfc.c" "$SRC/image/ez_source.c" \
    "$SRC/image/ez_encoder.c" "$SRC/image/ez_font.c"
python3 "$HERE/host/compare_precir.py" "$PRECIR/tools_python" "$OUT/ez_host" "$OUT"
python3 "$HERE/host/nfc_test.py" "$OUT/ez_host" "$OUT"
