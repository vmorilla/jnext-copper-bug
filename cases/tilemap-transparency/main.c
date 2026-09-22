// ---------------------------------------------------------------------------
// Minimal ZX Spectrum Next copper test case. The copper selects one of two
// tilemaps and changes its transparency index at the middle of the frame.
// ---------------------------------------------------------------------------

#include <arch/zxn.h>
#include <arch/zxn/copper.h>
#include <intrinsic.h>
#include <stdint.h>

// --- nextreg numbers/values not provided by the z88dk headers ---------------
#define REG_ULA_CONTROL 0x68
#define RUC_DISABLE_ULA_OUTPUT 0x80
#define REG_VERTICAL_LINE_COUNT_OFFSET 0x64

#define REG_TILEMAP_CONTROL 0x6B
#define RTC_ENABLE_TILEMAP 0x80
#define RTC_RESOLUTION_80x32 0x40
#define RTC_ELIMINATE_ATTRIBUTE_BYTE 0x20
#define REG_TILEMAP_DEFAULT_ATTRIBUTE 0x6C
#define REG_TILEMAP_BASE_ADDRESS 0x6E
#define REG_TILE_DEFINITIONS_BASE_ADDRESS 0x6F

// Raster line at which the copper selects the second tilemap.
// The 320x256 display area spans raster lines 32..287, so 160 is its middle.
#ifndef SWITCH_LINE
#define SWITCH_LINE 160
#endif

// The two 40x32 tilemaps are adjacent in bank 5. Each one uses one byte per
// entry because bit 5 of tilemap control eliminates the attribute byte.
#define TILEMAP_COLUMNS 40
#define TILEMAP_ROWS 32

#define TILE_INDEX_0 0 // game.fnt tile 0: all pixels palette index 0
#define TILE_INDEX_1 6 // game.fnt tile 6: all pixels palette index 1

#define TILEMAP_CONTROL (RTC_ENABLE_TILEMAP | RTC_ELIMINATE_ATTRIBUTE_BYTE)

// All live in bank 5, which the .nex loader maps at 0x4000..0x7FFF.
extern uint8_t tile_definitions[];
extern uint8_t tile_map_a[];
extern uint8_t tile_map_b[];

#define BANK_5_ORIGIN 0x4000

// Top half: tilemap A makes palette index 0 transparent, so its index 1 bars
// are yellow. Bottom half: tilemap B makes index 1 transparent, so its index 0
// bars are blue.
static uint16_t copper_tilemap_transparency[] = {
    CU_WAIT(0, 0),
    0,
    CU_MOVE(REG_TILEMAP_TRANSPARENCY_INDEX, 0),
    CU_MOVE(REG_TILEMAP_CONTROL, TILEMAP_CONTROL),
    CU_WAIT(SWITCH_LINE, 0),
    0,
    CU_MOVE(REG_TILEMAP_TRANSPARENCY_INDEX, 1),
    CU_MOVE(REG_TILEMAP_CONTROL, TILEMAP_CONTROL),
    CU_STOP};

// ---------------------------------------------------------------------------
// Setup helpers
// ---------------------------------------------------------------------------

static void setup_tilemaps(void)
{
    uint8_t *entry_a = tile_map_a;
    uint8_t *entry_b = tile_map_b;
    uint8_t row, col;

    ZXN_NEXTREGA(REG_TILE_DEFINITIONS_BASE_ADDRESS,
                 (uint8_t)(((uint16_t)tile_definitions - BANK_5_ORIGIN) >> 8));
    ZXN_NEXTREGA(REG_TILEMAP_DEFAULT_ATTRIBUTE, 0x00);
    copper_tilemap_transparency[1] = CU_MOVE(
        REG_TILEMAP_BASE_ADDRESS,
        (uint8_t)(((uint16_t)tile_map_a - BANK_5_ORIGIN) >> 8));
    copper_tilemap_transparency[5] = CU_MOVE(
        REG_TILEMAP_BASE_ADDRESS,
        (uint8_t)(((uint16_t)tile_map_b - BANK_5_ORIGIN) >> 8));

    ZXN_NEXTREGA(REG_PALETTE_CONTROL, RPC_SELECT_TILEMAP_PALETTE_0);
    ZXN_NEXTREGA(REG_PALETTE_INDEX, 0);
    ZXN_NEXTREGA(REG_PALETTE_VALUE_8, 0x03); // index 0 -> blue
    ZXN_NEXTREGA(REG_PALETTE_VALUE_8, 0xFC); // index 1 -> yellow
    ZXN_NEXTREGA(REG_PALETTE_CONTROL, RPC_SELECT_ULA_PALETTE_0);

    for (row = 0; row < TILEMAP_ROWS; row++)
        for (col = 0; col < TILEMAP_COLUMNS; col++)
        {
            *entry_a++ = (col & 4) ? TILE_INDEX_1 : TILE_INDEX_0;
            *entry_b++ = ((col + row) & 4) ? TILE_INDEX_1 : TILE_INDEX_0;
        }
}

static void copper_load_and_run(const uint16_t *program, uint16_t program_size)
{
    const uint8_t *byte = (const uint8_t *)program;
    uint16_t remaining = program_size;

    // Stop the copper and rewind its write pointer to address 0
    ZXN_NEXTREG(REG_COPPER_CONTROL_L, 0);
    ZXN_NEXTREG(REG_COPPER_CONTROL_H, 0);

    while (remaining--)
        ZXN_NEXTREGA(REG_COPPER_DATA, *byte++);

    // Restart the program from address 0 on every vertical blank
    ZXN_NEXTREG(REG_COPPER_CONTROL_L, 0);
    ZXN_NEXTREG(REG_COPPER_CONTROL_H, RCCH_COPPER_RUN_VBI);
}

static void test_tilemap_transparency(void)
{
    ZXN_NEXTREGA(REG_TILEMAP_BASE_ADDRESS,
                 (uint8_t)(((uint16_t)tile_map_a - BANK_5_ORIGIN) >> 8));
    ZXN_NEXTREGA(REG_TILEMAP_TRANSPARENCY_INDEX, 0);
    ZXN_NEXTREGA(REG_TILEMAP_CONTROL, TILEMAP_CONTROL);
    copper_load_and_run(copper_tilemap_transparency, sizeof(copper_tilemap_transparency));
}

int main(void)
{
    intrinsic_di();

    setup_tilemaps();

    // Hide the ULA layer so nothing else can explain what is seen
    ZXN_NEXTREGA(REG_ULA_CONTROL, RUC_DISABLE_ULA_OUTPUT);

    // Leave the copper's WAIT line numbers as raw raster lines
    ZXN_NEXTREGA(REG_VERTICAL_LINE_COUNT_OFFSET, 0);

    test_tilemap_transparency();

    while (1)
    {
    }
}
