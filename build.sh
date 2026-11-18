#!/bin/sh
# Builds q2-doom for the Shanling Q2 (Ingenic X2000, MIPS32r2 little-endian, glibc 2.28, o32
# with FR=1 and NaN2008, as Q2 Pod's own q2video is built) and lays out the card folder.
#
#   ./build.sh            -> build/q2-doom and dist/.doom/ (copy .doom to the card's root)
#   ./build.sh host       -> build/host/ : the host tests (test/run.sh)
#
# _IO_stdin_used is exported because glibc looks it up in the program: without it, glibc takes
# the program for a libc5-era one and gives it the old stdio. Needs clang, ld.lld and python3, no MIPS sysroot: sysroot/include declares what's used, and the
# stub libraries made by sysroot/stubs/gen.py carry the real sonames, so on the Q2 the rootfs's
# own libc, libpthread and alsa-lib are what run.
set -e
cd "$(dirname "$0")"

TARGET="--target=mipsel-linux-gnu -march=mips32r2 -mabi=32 -mfp64 -mnan=2008 -mabs=2008"
DOOMDEFS="-DNORMALUNIX -DLINUX -D_DEFAULT_SOURCE -DFEATURE_SOUND"
CFLAGS="$TARGET -mabicalls -fno-pic -G0 -nostdlibinc -isystem sysroot/include -O2 -fno-strict-aliasing \
        -ffunction-sections -fdata-sections -fno-stack-protector -fno-unwind-tables \
        -fno-asynchronous-unwind-tables $DOOMDEFS"
# -G0: no $gp-relative small data. glibc calls main with its own $gp (libc is PIC), so a
# gp-relative global in our non-PIC code would land in libc's memory.
# The engine is Chocolate Doom's 1990s C: its warnings are left alone. Ours aren't.
OURS="doomgeneric_q2 i_q2sound"
STRICT="-Wall -Wextra -Wno-unused-parameter -Werror"

mkdir -p build/obj
python3 sysroot/stubs/gen.py build/stubs

for src in src/*.c rt/*.c; do
    name=$(basename "$src" .c)
    obj=build/obj/$name.o
    flags="-w"
    case " $OURS " in *" $name "*) flags="$STRICT" ;; esac
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ] || [ build.sh -nt "$obj" ]; then
        clang $CFLAGS $flags -c "$src" -o "$obj"
    fi
done

ld.lld -m elf32ltsmip -e __start --dynamic-linker /lib/ld-linux-mipsn8.so.1 \
    --image-base=0x400000 -z noexecstack --gc-sections --hash-style=sysv --build-id=none -s \
    --export-dynamic-symbol=_IO_stdin_used \
    --as-needed build/obj/*.o build/stubs/libpthread.so.0 build/stubs/libasound.so.2 \
    build/stubs/libc.so.6 -o build/q2-doom

rm -rf dist
mkdir -p dist/.doom
cp build/q2-doom dist/.doom/q2-doom
cp card/q2app card/icon.png card/icon-40.png card/args.txt dist/.doom/
cp README.md dist/.doom/README.txt
echo "built: build/q2-doom ($(wc -c < build/q2-doom) bytes), card folder: dist/.doom"
