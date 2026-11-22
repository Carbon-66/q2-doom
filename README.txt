# ClassiCube for the Shanling Q2 (Q2 Pod)

ClassiCube is an open-source block-building game in the style of Minecraft Classic. This port
runs it as a Q2 Pod **card game**: it sits in **Library → Games** under Sudoku, Vortex, Solitaire
and Snake, and runs full screen. Rockbox is never involved. Everything it needs is in this folder,
textures included, so it never goes online.

## Install

1. Copy the `.classicube` folder to the **root of the card** (next to your music folders). On a
   Mac, press ⌘⇧. in Finder to see dot-folders.
2. Eject the card, then open **Library → Games → ClassiCube**.

The first start makes a new world. It's saved when you leave and every five minutes, and the
next start carries on where you were.

## Controls

The Q2's touchscreen takes one finger at a time, so the wheel does the turning.

| Control | In the game | In a menu or the inventory |
| --- | --- | --- |
| **Wheel** | Turn (anticlockwise turns left) | Scroll |
| **Centre** | Jump (swim up in water) | Enter |
| **Return** | Pause menu | Back |
| **Hold Return** | Save and leave (Q2 Pod's own) | Save and leave |
| **Touch: the stick, bottom left** | Hold and drag up to walk, down to back off, sideways to strafe | |
| **Touch: drag anywhere else** | Look around (up and down too) | |
| **Touch: tap** | Place a block | Select |
| **Touch: hold** | Break a block | |
| **Touch: the hotbar** | Pick a block; tap ⋯ at its end for the inventory | |
| **Touch: ⋯ at the top** | More: Fog (cycles the view distance), Controls (tap and hold actions, button size), Main menu, Chat, Screenshot | |

**Typing** (a level's name, chat, a number in Options) brings up an on-screen keyboard or numpad.
The **wheel** moves the highlight from key to key, in reading order (clockwise forward), and
**Centre** presses the highlighted key. Its **Enter** finishes: it presses OK, or sends the chat
message. **Return** closes the keyboard. Tapping keys works too.

**Fly** and **Speed** (top right) turn on flying and running. In flight, ▲ and ▼ appear bottom
right to go up and down.

Play/Pause, Previous and Next stay the music player's, so your music plays on while you build.

**Quit game** in the pause menu, or holding Return, saves the world and goes back to Games. Five
minutes without input does the same, so a forgotten game doesn't keep the screen lit.

## Size on the small screen

Menus, buttons and text are drawn larger than ClassiCube's other small-screen ports, as large as
its two-column menus allow (they then span the screen's width). The hotbar and inventory are
2.5×, menu buttons are thicker, and the on-screen keyboard's keys are enlarged. **Use system
font** (Gui options) is on: ClassiCube's own larger, clearer lettering. **Options… → Gui options…** has Hotbar scale
and Inventory scale. The overall size is `q2-uiscale` in `options.txt` (0.58; 0.5 is smaller,
and above about 0.6 the menus run off the screen).

## Speed

The Q2 has no graphics chip: ClassiCube draws every frame on the CPU. The view distance starts at
32 blocks to keep it smooth. **Pause menu → Options… → Graphics options… → View distance**, or
⋯ → Fog, changes it: shorter is faster.

## Files

Everything ClassiCube keeps is in this folder:

| File | What it is |
| --- | --- |
| `maps/q2-world.cw` | Your world. Delete it for a fresh one at the next start, or use **Pause → Generate new level**. |
| `maps/` | Also levels you save with **Save level** (ClassiCube's own `.cw` files work on a computer too). |
| `texpacks/default.zip` | ClassiCube's own textures. Another texture pack in `texpacks/` can be picked in **Change texture pack**. |
| `options.txt` | Settings, written as you change them. |
| `classicube.log` | What the last run printed. Check it if something goes wrong. |

## How it fits Q2 Pod

`q2app` says `game=1` (a full-screen card game in Games). Q2 Pod forks `.classicube/q2-classicube`
and stops painting while it runs:

- **Screen.** ClassiCube's software renderer draws 375 × 320 frames, which are turned onto the
  320 × 375 portrait panel (`/dev/fb0`), drawn on the hidden page and panned, as Q2 Pod's video
  player does. The touch buttons are kept 24 px in from the edges, clear of the glass's corners.
- **Input.** Q2 Pod sends text datagrams to `/tmp/q2-classicube.input.sock`: `k d|u CODE` for
  keys, `t d|m|u X Y` for touch and `w N` for the wheel's travel.
- **Sound.** None yet: there is no `audio=1`, so Q2 Pod keeps the sound output (and your music).

## Licence

ClassiCube is © UnknownShadow200 and its contributors, under the BSD 3-Clause licence
(`LICENSE-ClassiCube.txt`). The textures are ClassiCube's own pack from its repository
(`misc/cc_textures.zip`). The Q2 port is under the same BSD 3-Clause licence.
