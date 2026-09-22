; Storage in bank 5 (mapped at 0x4000..0x7FFF) for the tilemap test.
;
;   0x6000 .. 0x70A0   tile definitions: 133 tiles, 4bpp, 32 bytes each
;   0x7100 .. 0x7600   tilemap A: 40 columns x 32 rows, 1 byte per tile
;   0x7600 .. 0x7B00   tilemap B: 40 columns x 32 rows, 1 byte per tile
;
; Both bases must sit on a 256 byte boundary: nextreg 0x6E/0x6F only carry the
; MSB of the offset into the bank.

SECTION BANK_5_TILEDEFS

PUBLIC _tile_definitions

ORG 0x6000

_tile_definitions:
    incbin "game.fnt"

SECTION BANK_5_TILEMAP

PUBLIC _tile_map_a
PUBLIC _tile_map_b

ORG 0x7100

_tile_map_a:
    defs 40 * 32

_tile_map_b:
    defs 40 * 32
