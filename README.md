# Doom for the Shanling Q2 (Q2 Pod)

Doom runs as a Q2 Pod **card game**. It sits in **Library → Games** under Sudoku, Vortex, Solitaire and Snake, and runs full screen. Rockbox is never involved. It's [doomgeneric](https://github.com/ozkl/doomgeneric) (Chocolate Doom's engine), taken from [doom-on-router](https://github.com/antonKirpich/doom-on-router)'s MIPS build. It has a Q2 backend: the screen, the touch wheel, the buttons, the touchscreen and sound effects.

## Install

1. Copy the `.doom` folder to the **root of the card** (next to your music folders). On a Mac, press ⌘⇧. in Finder to see dot-folders.
2. Put a Doom game file (IWAD) in `.doom`. Any one of these MAY work:
   - `DOOM1.WAD`: the free shareware episode (tested to work)
   - `DOOM.WAD`, `DOOM2.WAD`, `TNT.WAD`, `PLUTONIA.WAD`: from your copy of Doom
   - `FREEDOOM1.WAD`, `FREEDOOM2.WAD`: free, from [freedoom.github.io](https://freedoom.github.io)

   You can also put the WAD in a visible `Doom` folder at the card's root.
3. Eject the card, then open **Library → Games → Doom**.

Without a WAD, Doom shows a screen saying what to copy. Any key goes back.

## Controls

| Control | In the game | In menus |
| --- | --- | --- |
| **Wheel** | Turn (spin anticlockwise to turn left) | Up and down |
| **Centre** | Fire | Select; **yes** on a Y/N question |
| **Return** | Menu | Back; **no** on a Y/N question |
| **Hold Return** | Leave Doom (Q2 Pod's own) | Leave Doom |
| **Play/Pause** | Use (doors, switches, lifts) | Select |
| **Previous** | Automap | Back |
| **Next** | Next weapon | Select |
| **Touchscreen** | A floating stick: put a thumb down anywhere, then drag up to walk, down to back off, sideways to strafe, and further to run. A quick tap is **Use**. | A tap selects |

A comfortable hold is: right thumb on the wheel and Centre, left thumb on the screen.

**Quit Game** in Doom's menu (Centre to confirm) or holding Return both go back to Games. Doom saves its settings when it closes. Five minutes without input also ends Doom, so a forgotten game doesn't keep the screen lit.

Wheel turning speed follows **Options → Mouse sensitivity**.

## Sound

Sound effects play through the headphone jack, Bluetooth or a USB DAC, whichever Q2 Pod is using. They follow Mono Audio and Swap L/R. Music stops while Doom runs, as it does for a video. Doom's own music is **not** played yet: it needs an OPL synthesiser.

## Options

Files in `.doom` you can add or edit:

| File | What it does |
| --- | --- |
| `args.txt` | Extra Doom arguments, such as `-file sigil.wad`, `-skill 4` or `-warp 1 1`. The file has examples. |
| `fullscreen` | An empty file with this name fills the screen's width (375 × 281) instead of 320 × 240. The glass's rounded corners then cut off the status bar's corners. |
| `default.cfg` | Doom's settings, written when it closes. `usegamma 1` to `4` brightens the picture. |
| `savegames/` | Your saved games. |
| `doom.log` | What Doom printed on its last run. Check it if something goes wrong. |

## How it fits Q2 Pod

`q2app` says `game=1` (a full-screen card game in Games) and `audio=1` (Doom gets the sound output and the Play/Pause, Previous and Next buttons). Q2 Pod forks `.doom/q2-doom` and stops painting while it runs:

- **Screen.** Doom draws on `/dev/fb0`, a 320 × 375 portrait panel with two pages. Each 320 × 200 frame is scaled to 4:3 (320 × 240), turned onto the portrait panel, drawn on the hidden page and panned, as Q2 Pod's video player does.
- **Input.** Q2 Pod sends text datagrams to `/tmp/q2-doom.input.sock`: `k d|u CODE` for keys, `t d|m|u X Y` for touch, and `w N` for the wheel's travel.
- **Sound.** The ALSA device (and, off the jack, the volume) comes as arguments, opened the way Q2 Pod's own `q2video` opens it.

## Building

```sh
./build.sh          # build/q2-doom and dist/.doom (the card folder)
./test/run.sh       # host tests: picture placement, controls, mixer, and main() on a fake screen
```

Needs `clang`, `ld.lld` and `python3` (with Pillow for `tools/icon.py` and `test/view.py`). There's no MIPS sysroot or cross-gcc. The build matches the ABI of Q2 Pod's own helpers: MIPS32r2 little-endian, o32, FR=1, NaN2008, glibc 2.28, interpreter `/lib/ld-linux-mipsn8.so.1`. `sysroot/include` declares the libc that Doom uses. `sysroot/stubs/gen.py` makes stub `libc.so.6`, `libpthread.so.0` and `libasound.so.2` to link against, so the Q2's own libraries are what run. If you add a call to a libc function, add it to `gen.py` too.

Changes to the engine are marked `q2-doom` in the source:

- `I_Quit` exits.
- `I_Error` shows its message on screen.
- Enter answers a Y/N prompt.
- The zone is 16 MB.
- The config and saves are `default.cfg` and `savegames/`.

## Licence

GPL. The engine is GPL-2.0-or-later (Chocolate Doom, doomgeneric). This port follows Q2 Pod's helper contract and is distributed under GPL-3.0-or-later. See `LICENSE`.
