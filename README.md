# jnext copper test cases

Minimal ZX Spectrum Next programs that exercise the copper, each isolating one
rendering behaviour in [jnext](https://github.com/jorgegv/jnext).

| case | what it shows | status on jnext 0.99.155 |
| ---- | ------------- | ------------------------ |
| [layer2-bank-midline](cases/layer2-bank-midline/) | a copper `MOVE` to the Layer 2 bank (nextreg `0x12`) part-way along a scanline is applied to the whole line | **reproduces** |
| [tilemap-transparency](cases/tilemap-transparency/) | a copper `MOVE` to the tilemap transparency index (nextreg `0x4C`) was not honoured mid-frame | **fixed** — kept as a regression case |

Each case directory has its own README with the expected and actual output.

## Running a case

Requires [z88dk](https://z88dk.org) (`zcc` on the `PATH`) and jnext.

```sh
make list                        # the cases, and which one CASE defaults to
make CASE=<name>                 # build build/<name>.nex
make CASE=<name> jnext           # run it
make CASE=<name> shot            # deterministic headless PNG into build/
make clean
```

`make shot` pins the RTC and schedules the capture in emulated frames, so two
runs of the same build produce byte-identical PNGs — which is how the images in
each case directory were made, and what makes "did this change?" answerable
with `cmp`.

`make CASE=<name> mame` copies the program onto a Next SD card image and starts
MAME on it. It expects the image at `~/bin/next-images/cspect-next-2gb.img`
plus `mtools` and [`txt2bas`](https://github.com/remy/txt2bas); adjust the
variables at the top of the [Makefile](Makefile). Note that MAME has to boot
NextZXOS and load through `.nexload`, so it is slow — slow enough that the
reference capture for the newer case has not been taken this way.

## Adding a case

Create `cases/<name>/` with a `main.c` (and any `.asm` it needs) and a
`README.md`. The Makefile globs the directory, so nothing else has to be
registered.
