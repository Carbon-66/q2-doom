#!/bin/sh
# Host tests: q2-doom's backend and sound module built for this machine with the whole engine,
# then (1) unit.c's checks and (2) the real main() under fbshim.c's fake /dev/fb0, in a folder
# laid out like the card, with no WAD: it must show its message, take Centre to leave, and put
# the panel back on demo's page. Writes test/out/*.raw for a look (test/view.py).
set -e
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p $OUT/obj test/out
CF="-O1 -g -w -fno-strict-aliasing -DNORMALUNIX -DLINUX -D_DEFAULT_SOURCE -DFEATURE_SOUND -Isrc"
for src in src/*.c; do
    name=$(basename $src .c)
    [ "$name" = doomgeneric_q2 ] && continue
    obj=$OUT/obj/$name.o
    [ -f $obj ] && [ $obj -nt $src ] || clang $CF -c $src -o $obj
done
clang $CF -Wall -Wextra -Wno-unused-parameter -Werror -c src/doomgeneric_q2.c -o $OUT/q2main.o
clang $CF -Dmain=q2doom_main -c src/doomgeneric_q2.c -o $OUT/q2unit.o
clang $CF -c test/alsa_fake.c -o $OUT/alsa_fake.o
clang $CF -c test/unit.c -o $OUT/unit.o
clang -pthread $OUT/obj/*.o $OUT/q2unit.o $OUT/alsa_fake.o $OUT/unit.o -o $OUT/unit
clang -pthread $OUT/obj/*.o $OUT/q2main.o $OUT/alsa_fake.o -o $OUT/q2-doom
clang -shared -fPIC -O1 test/fbshim.c -o $OUT/fbshim.so -ldl

echo "== unit"
$OUT/unit

echo "== main(), no WAD"
CARD=$(mktemp -d)
mkdir -p $CARD/.doom
cp $OUT/q2-doom $CARD/.doom/q2-doom
rm -f /tmp/q2-doom.input.sock
( FB_DUMP=$PWD/test/out/nowad.raw FB_FINAL=$PWD/test/out/nowad.final LD_PRELOAD=$PWD/$OUT/fbshim.so \
    $CARD/.doom/q2-doom plughw:0,0 & echo $! > $CARD/pid; wait $! ; echo $? > $CARD/status ) &
sleep 1
[ -S /tmp/q2-doom.input.sock ] || { echo "FAIL: no input socket"; exit 1; }
python3 -c "import socket; s=socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM); s.sendto(b'k d 218', '/tmp/q2-doom.input.sock')"
sleep 1
status=$(cat $CARD/status 2>/dev/null || echo running)
echo "exit status: $status"; [ "$status" = 0 ] || { echo "FAIL: didn't leave on Centre"; exit 1; }
[ ! -e /tmp/q2-doom.input.sock ] || { echo "FAIL: socket left behind"; exit 1; }
grep -q "yoffset 0 " test/out/nowad.final || { echo "FAIL: panel not back on demo's page: $(cat test/out/nowad.final)"; exit 1; }
[ -f $CARD/.doom/doom.log ] || { echo "FAIL: no doom.log"; exit 1; }
echo "main: ok ($(cat test/out/nowad.final))"
rm -rf $CARD

# With a WAD (Q2DOOM_WAD=path/to/doom1.wad test/run.sh): the game starts, draws frames, saves its
# settings when TERMed, and loads them back.
if [ -n "$Q2DOOM_WAD" ]; then
    echo "== main(), $(basename "$Q2DOOM_WAD")"
    CARD=$(mktemp -d)
    mkdir -p $CARD/.doom
    cp $OUT/q2-doom $CARD/.doom/q2-doom
    cp "$Q2DOOM_WAD" $CARD/.doom/DOOM1.wad   # upper case, as a card copied from Windows has it
    for run in 1 2; do
        FB_FINAL=$PWD/test/out/wad.final FB_DUMP=$PWD/test/out/wad.raw LD_PRELOAD=$PWD/$OUT/fbshim.so \
            $CARD/.doom/q2-doom plughw:0,0 & P=$!
        sleep 3; kill -TERM $P; wait $P || { echo "FAIL: exit status $?"; exit 1; }
        pans=$(sed 's/.*pans //' test/out/wad.final)
        [ "$pans" -gt 20 ] || { echo "FAIL: only $pans frames"; exit 1; }
        [ $run = 1 ] && sed -i 's/^mouse_sensitivity .*/mouse_sensitivity 7/' $CARD/.doom/default.cfg
    done
    grep -q "^mouse_sensitivity  *7" $CARD/.doom/default.cfg || { echo "FAIL: settings not kept"; exit 1; }
    grep -q "^key_fire  *163" $CARD/.doom/default.cfg || { echo "FAIL: fire unbound"; exit 1; }
    echo "game: ok ($pans frames in 3 s, settings kept)"
    rm -rf $CARD
fi
