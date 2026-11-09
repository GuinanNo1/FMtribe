#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal, Ian Guinan
# FMtribe's tests: the engine against the recordings' estimates (tests/host/tribe_test.c: filter, VCO, EGs, drums; its
# renders in app/build/tests/tribe/), the whole app in fm1-base's host simulator (app/tests/basic.txt: keys, MIDI,
# settings across a reboot, a render to WAV, the screen reader; app/tests/sound.txt: the panel, the drums, the MIDI
# map), the update path against FMtribe's package when ./build.sh has made
# one, and the browser build when Emscripten is installed. Screenshots and audio in app/build/tests/.
# fm1-base's own tests: sh base/tests/run_tests.sh
#   sh tests/run_tests.sh
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
PY="${PYTHON:-python3}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
B=app/build
OUT=$B/tests
FAIL=0
mkdir -p "$OUT"
run() {
    name="$1"; shift
    if "$@" > "$OUT/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see $OUT/$name.log)"
        tail -15 "$OUT/$name.log" | sed 's/^/       /'
        FAIL=1
    fi
}
if [ -f $B/fmtribe.fwsc ]; then
    run ota-entry sh -c "$CC -w -o $OUT/ota_test base/tests/ota_test.c && $OUT/ota_test $B/fmtribe.fwsc"
    run update-loader sh -c "head -c 100000 $B/fmtribe.bin > $OUT/old_app.bin && \
        $PY base/tools/fm1pkg_make.py $OUT/old_app.bin $B/loader/ota.bin $OUT/old.fwsc >/dev/null && \
        $CC -w -o $OUT/ldr_test base/tests/ldr_test.c && $OUT/ldr_test $OUT/old.fwsc $B/fmtribe.fwsc"
else
    echo "  skip update path (no $B/fmtribe.fwsc: ./build.sh)"
fi
mkdir -p "$OUT/tribe"
run tribe sh -c "$CC -O2 -ffp-contract=off -Ibase/include -Iapp/dsp -o $OUT/tribe_test tests/host/tribe_test.c -lm && \
    $OUT/tribe_test $OUT/tribe"
run seq sh -c "$CC -O2 -Iapp/seq -o $OUT/seq_test tests/host/seq_test.c -lm && $OUT/seq_test"
run host-build sh build.sh --host
mkdir -p "$OUT/basic" "$OUT/sound" "$OUT/seq"
run basic $B/host/fmtribe_host app/tests/basic.txt "$OUT/basic"
run sound $B/host/fmtribe_host app/tests/sound.txt "$OUT/sound"
run seq-panel $B/host/fmtribe_host app/tests/seq.txt "$OUT/seq"
if command -v emcc >/dev/null 2>&1; then
    run emu sh build.sh --web
else
    echo "  skip emu (no emcc)"
fi
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
