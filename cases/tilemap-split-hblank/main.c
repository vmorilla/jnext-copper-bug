// ---------------------------------------------------------------------------
// Minimal ZX Spectrum Next copper test case, modelled on the controls menu of
// next-point. At two raster lines, with the beam past the right edge of the
// display, the copper switches the tilemap tile definitions (0x6F), the
// tilemap transparency index (0x4C) and the tilemap control (0x6B) together.
//
// The tile definitions and the transparency index are chosen so that the
// tilemap is fully transparent as long as the three writes take effect on the
// same raster line: the screen must be solid black. Any line on which the new
// tile definitions and the old transparency index (or vice versa) meet is
// drawn in pink.
// ---------------------------------------------------------------------------

#include <arch/zxn.h>
#include <arch/zxn/copper.h>
#include <intrinsic.h>
#include <stdint.h>
#include <string.h>

// --- nextreg numbers/values not provided by the z88dk headers ---------------
#define REG_ULA_CONTROL 0x68
#define RUC_DISABLE_ULA_OUTPUT 0x80
#define REG_VERTICAL_LINE_COUNT_OFFSET 0x64
#define REG_FALLBACK_COLOUR 0x4A

#define REG_TILEMAP_CONTROL 0x6B
#define RTC_ENABLE_TILEMAP 0x80
#define RTC_80x32_MODE 0x40
#define REG_TILEMAP_DEFAULT_ATTRIBUTE 0x6C
#define REG_TILEMAP_BASE_ADDRESS 0x6E
#define REG_TILE_DEFINITIONS_BASE_ADDRESS 0x6F

// The same lines and horizontal position as next-point's controls menu:
//   CU_WAIT((29 - 4 * 2) * 8 - 1, 320 >> 3)  and  CU_WAIT((29 + 2) * 8, 320 >> 3)
#ifndef SPLIT_LINE_1
#define SPLIT_LINE_1 167
#endif
#ifndef SPLIT_LINE_2
#define SPLIT_LINE_2 248
#endif
#ifndef SPLIT_HPOS
#define SPLIT_HPOS (320 >> 3)
#endif

// Copper vertical line offset, as nextreg 0x64. next-point uses 32.
#ifndef COPPER_LINE_OFFSET
#define COPPER_LINE_OFFSET 32
#endif

// CPU speed, as nextreg 0x07. next-point runs at 28 MHz.
#ifndef CPU_SPEED
#define CPU_SPEED 3
#endif

// Everything lives in bank 5, which the .nex loader maps at 0x4000..0x7FFF.
// The ULA is disabled, so the screen memory is free. Both bases must be on a
// 256 byte boundary: nextreg 0x6E/0x6F only hold the MSB of the offset.
#define BANK_5_ORIGIN 0x4000
#define TILE_MAP ((uint8_t *)0x6000)    // 80x32 entries x 2 bytes = 0x1400
#define TILE_DEFS_A ((uint8_t *)0x7400) // tile 0: every pixel is index 0
#define TILE_DEFS_B ((uint8_t *)0x7500) // tile 0: every pixel is index 1
#define TILE_MAP_SIZE (80 * 32 * 2)
#define TILE_SIZE 32

#define MSB_OFFSET(p) ((uint8_t)(((uint16_t)(p) - BANK_5_ORIGIN) >> 8))

// The transparency index inside the split. Building with -DSPLIT_TRANSPARENCY=0
// is a sanity check: the band between the two splits must then be pink.
#ifndef SPLIT_TRANSPARENCY
#define SPLIT_TRANSPARENCY 1
#endif

#define PINK 0xE7 // not 0xE3, so that it is not the global transparent colour

// Outside the split: definitions A with index 0 transparent, 40x32.
// Inside the split: definitions B with index 1 transparent, 80x32.
// Either way, every tilemap pixel is transparent.
static const uint16_t copper_program[] = {
    CU_WAIT(SPLIT_LINE_1, SPLIT_HPOS),
    CU_MOVE(REG_TILE_DEFINITIONS_BASE_ADDRESS, MSB_OFFSET(TILE_DEFS_B)),
    CU_MOVE(REG_TILEMAP_TRANSPARENCY_INDEX, SPLIT_TRANSPARENCY),
    CU_MOVE(REG_TILEMAP_CONTROL, RTC_ENABLE_TILEMAP | RTC_80x32_MODE),
    CU_WAIT(SPLIT_LINE_2, SPLIT_HPOS),
    CU_MOVE(REG_TILE_DEFINITIONS_BASE_ADDRESS, MSB_OFFSET(TILE_DEFS_A)),
    CU_MOVE(REG_TILEMAP_TRANSPARENCY_INDEX, 0),
    CU_MOVE(REG_TILEMAP_CONTROL, RTC_ENABLE_TILEMAP),
    CU_STOP};

static void setup_tilemap(void)
{
    memset(TILE_MAP, 0, TILE_MAP_SIZE);       // tile 0, attribute 0 everywhere
    memset(TILE_DEFS_A, 0x00, TILE_SIZE);     // 4bpp: all pixels index 0
    memset(TILE_DEFS_B, 0x11, TILE_SIZE);     // 4bpp: all pixels index 1

    ZXN_NEXTREGA(REG_PALETTE_CONTROL, RPC_SELECT_TILEMAP_PALETTE_0);
    ZXN_NEXTREGA(REG_PALETTE_INDEX, 0);
    ZXN_NEXTREGA(REG_PALETTE_VALUE_8, PINK); // index 0
    ZXN_NEXTREGA(REG_PALETTE_VALUE_8, PINK); // index 1
    ZXN_NEXTREGA(REG_PALETTE_CONTROL, RPC_SELECT_ULA_PALETTE_0);

    ZXN_NEXTREGA(REG_TILEMAP_BASE_ADDRESS, MSB_OFFSET(TILE_MAP));
    ZXN_NEXTREGA(REG_TILEMAP_DEFAULT_ATTRIBUTE, 0x00);

    // The state the copper restores at SPLIT_LINE_2
    ZXN_NEXTREGA(REG_TILE_DEFINITIONS_BASE_ADDRESS, MSB_OFFSET(TILE_DEFS_A));
    ZXN_NEXTREGA(REG_TILEMAP_TRANSPARENCY_INDEX, 0);
    ZXN_NEXTREGA(REG_TILEMAP_CONTROL, RTC_ENABLE_TILEMAP);
}

static void copper_load_and_run(const uint16_t *program, uint16_t program_size)
{
    const uint8_t *byte = (const uint8_t *)program;

    // Stop the copper and rewind its write pointer to address 0
    ZXN_NEXTREG(REG_COPPER_CONTROL_L, 0);
    ZXN_NEXTREG(REG_COPPER_CONTROL_H, 0);

    while (program_size--)
        ZXN_NEXTREGA(REG_COPPER_DATA, *byte++);

    // Restart the program from address 0 on every vertical blank
    ZXN_NEXTREG(REG_COPPER_CONTROL_L, 0);
    ZXN_NEXTREG(REG_COPPER_CONTROL_H, RCCH_COPPER_RUN_VBI);
}

// spin.asm: an endless loop whose length does not divide the frame, so the
// CPU's instruction boundaries drift against the raster from frame to frame,
// as they do in any real program.
extern void spin(void);

int main(void)
{
    intrinsic_di();

    ZXN_NEXTREGA(REG_TURBO_MODE, CPU_SPEED);

    // Only the tilemap is on screen; what is behind it is the fallback colour
    ZXN_NEXTREGA(REG_ULA_CONTROL, RUC_DISABLE_ULA_OUTPUT);
    ZXN_NEXTREGA(REG_FALLBACK_COLOUR, 0x00);

    // As next-point does: an offset of 32 makes copper line 0 the first line
    // of the 320x256 tilemap area, so WAIT line n is framebuffer row n
    ZXN_NEXTREGA(REG_VERTICAL_LINE_COUNT_OFFSET, COPPER_LINE_OFFSET);

    setup_tilemap();
    copper_load_and_run(copper_program, sizeof(copper_program));

    spin();
    return 0;
}
