# jnext bug: a copper MOVE part-way along a scanline is applied to the whole line

**Status: reproduces on jnext 0.99.155.**

A copper `MOVE` to the Layer 2 active bank register (nextreg `0x12`) that lands
part-way along a scanline is not honoured at the point it is written. The whole
scanline is drawn with the **last** value written on it, so the pixels to the
left of the second `MOVE` — which should come from the first bank — come from
the second one instead.

Switching the bank *between* lines works correctly, which is what makes this
specific: the copper is running, the register reaches Layer 2, and the change
takes effect at the right raster line. Only its horizontal position is lost.

## What the test program does

[main.c](main.c) paints two Layer 2 screens, each three 16K banks, in flat
colour: bank 8 (the machine's default active bank) red, bank 11 (the default
shadow bank) green. The ULA layer is switched off so nothing else is on screen,
and the default Layer 2 palette is `RRRGGGBB`, so no palette setup is needed.

The copper list then runs once per frame, restarted at each vertical blank
(nextreg `0x62` = `0xC0`):

| lines | copper writes | expected |
| ----- | ------------- | -------- |
| 0     | `0x12` = 8 once | lines 0–47 solid **red** — the control |
| 48–191 | per line: `0x12` = 8 at hpos 8, `0x12` = 11 at hpos 32 | each line **red** from hpos 8 to hpos 32, then **green** |

The control band is the part that matters for reading the result: it uses the
same register, written by the same copper, and it comes out right.

## Expected result

Three regions: a red band at the top, then a band whose left part is red and
whose right part is green, repeated on every line.

## Actual result in jnext — [jnext.png](jnext.png)

<img src="jnext.png" width="480" alt="jnext: a red band at the top, everything below solid green">

The control band is **red**, so per-line bank switching is correct. Everything
below line 48 is **solid green** — not one red pixel. The `MOVE` to bank 8 at
hpos 8 has no effect at all; the line is drawn entirely with bank 11, the value
left by the `MOVE` at hpos 32.

Scanline sample from the capture (`make shot`, 640×512, 2× scale):

```
y=153  (control band)   black×64  RED  ×512  black×64      <- correct
y=230  (switched band)  black×64  GREEN×512  black×64      <- should be RED then GREEN
```

## Where it comes from

The copper is not at fault. `Copper::execute` compares
`hc >= (hpos << 3) + 12` ([src/peripheral/copper.cpp:35,143](../../../jnext/src/peripheral/copper.cpp))
and issues the `MOVE` through `nextreg.write()`, which reaches the nextreg
`0x12` handler and `Layer2::set_active_bank`
([src/core/emulator.cpp:1620](../../../jnext/src/core/emulator.cpp)).

The value is then logged for the current scanline **with no horizontal
position** — [src/video/layer2.h:363](../../../jnext/src/video/layer2.h):

```cpp
struct BankChange {
    uint16_t line;
    uint8_t  active_bank;
    uint8_t  shadow_bank;
};
```

and the renderer applies every entry tagged with a line before drawing it —
[src/video/layer2.cpp:238](../../../jnext/src/video/layer2.cpp):

```cpp
while (bank_render_cursor_ < bank_change_count_
    && bank_change_log_[bank_render_cursor_].line == lt) {
    const auto& c = bank_change_log_[bank_render_cursor_++];
    active_bank_ = c.active_bank;        // last write on the line wins
    shadow_bank_ = c.shadow_bank;
}
```

`Renderer::render_frame` then calls `layer2.apply_changes_for_line(row)`
followed by `layer2.render_scanline(...)`, which draws all of the line from the
single surviving bank. Two writes on one line therefore collapse to the second.

The scroll (`0x16`/`0x17`) and clip-window logs next to it have the same shape,
so the same limitation should apply to a mid-line scroll change.

Fixing it means carrying the horizontal position into the log and having
`render_scanline` draw the line in segments rather than in one piece.

## Why it matters

It is not a corner case. This is how the Next's copper is normally used to put
a strip of a different Layer 2 screen inside a line — for example the moving
advertising hoardings in *next-point*, whose copper does exactly this on each
of the twelve lines of the ad strip:

```c
CU_WAIT(line, 2),                                  /* hoarding starts */
CU_MOVE(REG_LAYER_2_RAM_BANK, ad_bank),
CU_MOVE(REG_LAYER_2_OFFSET_Y, ad_offset),
CU_WAIT(line, 29),                                 /* hoarding ends   */
CU_MOVE(REG_LAYER_2_OFFSET_Y, 0),
CU_MOVE(REG_LAYER_2_RAM_BANK, crowd_bank)
```

Under jnext the strip is drawn entirely from `crowd_bank`, the second write, so
the hoardings never appear — the crowd is shown where they should be.

## Building and running

Requires [z88dk](https://z88dk.org) (`zcc` on the `PATH`) and jnext.

```sh
make CASE=layer2-bank-midline          # build build/layer2-bank-midline.nex
make CASE=layer2-bank-midline jnext    # run it
make CASE=layer2-bank-midline shot     # deterministic headless PNG
make CASE=layer2-bank-midline mame     # run it in MAME (needs an SD image)
```

The geometry can be moved for further testing:

```sh
make clean && make EXTRA_FLAGS="-DSPLIT_LINE=96 -DSWITCH_ON=4 -DSWITCH_OFF=48"
```
