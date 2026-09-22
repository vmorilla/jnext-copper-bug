/*
 * jnext bug: a copper MOVE to the Layer 2 bank register (nextreg 0x12) that
 * lands PART WAY ALONG a scanline is not honoured. The whole line is drawn
 * with the last value written on it.
 *
 * The program paints two Layer 2 screens in different colours and uses the
 * copper to switch between them twice on every line of the lower band. On
 * real hardware each of those lines is half one screen and half the other.
 * In jnext the whole band comes out in the second colour.
 *
 * The upper band is the control: there the bank is switched once, at the top
 * of the frame, and jnext gets that right -- which is what shows the copper is
 * running and nextreg 0x12 is reaching Layer 2.
 */
#include <arch/zxn.h>
#include <arch/zxn/copper.h>
#include <string.h>

/* Layer 2, 256x192, 8bpp: three 16K banks per screen. 8 and 11 are the
   machine's own active and shadow defaults, so nothing else has to move. */
#define SCREEN_A_BANK 8
#define SCREEN_B_BANK 11

/* Default Layer 2 palette is RRRGGGBB, so the colours need no palette setup. */
#define COLOUR_A 0xE0   /* red   */
#define COLOUR_B 0x1C   /* green */

/* The line the mid-line switching starts at. Above it, the control band. */
#ifndef SPLIT_LINE
#define SPLIT_LINE 48
#endif
#define LAST_LINE 191

/* Where on the line the copper switches, in the copper's own 8-pixel columns.
   jnext turns these into a pixel threshold of (hpos << 3) + 12. */
#ifndef SWITCH_ON
#define SWITCH_ON 8
#endif
#ifndef SWITCH_OFF
#define SWITCH_OFF 32
#endif

/* 2 for the control band + 4 per switched line + CU_STOP. */
#define COPPER_WORDS (2 + (LAST_LINE - SPLIT_LINE + 1) * 4 + 1)

static uint16_t copper_program[COPPER_WORDS];

/* Fills one 16K Layer 2 bank with a single colour, through MMU slot 6.
   A 16K bank is two 8K pages: bank N is pages 2N and 2N+1. */
static void fill_bank(uint8_t bank, uint8_t colour)
{
    uint8_t page;

    for (page = 0; page < 2; page++)
    {
        ZXN_WRITE_REG(0x56, (uint8_t)(bank * 2 + page));   /* MMU6 <- page */
        memset((void *)0xC000, colour, 0x2000);
    }
}

static void build_copper(void)
{
    uint16_t *p = copper_program;
    uint16_t line;

    /* Control band: one switch, at the very top of the frame. Nothing after
       this touches the bank until SPLIT_LINE, so lines 0..SPLIT_LINE-1 are
       screen A throughout. */
    *p++ = CU_WAIT(0, 0);
    *p++ = CU_MOVE(0x12, SCREEN_A_BANK);

    /* Test band: two switches on every line. */
    for (line = SPLIT_LINE; line <= LAST_LINE; line++)
    {
        *p++ = CU_WAIT(line, SWITCH_ON);
        *p++ = CU_MOVE(0x12, SCREEN_A_BANK);
        *p++ = CU_WAIT(line, SWITCH_OFF);
        *p++ = CU_MOVE(0x12, SCREEN_B_BANK);
    }

    *p++ = CU_STOP;
}

static void copper_load_and_start(void)
{
    const uint8_t *bytes = (const uint8_t *)copper_program;
    uint16_t i;

    /* Stop the copper and rewind its instruction pointer before loading. */
    ZXN_WRITE_REG(0x61, 0);
    ZXN_WRITE_REG(0x62, 0);

    for (i = 0; i < sizeof(copper_program); i++)
        ZXN_WRITE_REG(0x60, bytes[i]);

    /* Restart the list at every vertical blank. */
    ZXN_WRITE_REG(0x61, 0);
    ZXN_WRITE_REG(0x62, 0xC0);
}

int main(void)
{
    fill_bank(SCREEN_A_BANK + 0, COLOUR_A);
    fill_bank(SCREEN_A_BANK + 1, COLOUR_A);
    fill_bank(SCREEN_A_BANK + 2, COLOUR_A);
    fill_bank(SCREEN_B_BANK + 0, COLOUR_B);
    fill_bank(SCREEN_B_BANK + 1, COLOUR_B);
    fill_bank(SCREEN_B_BANK + 2, COLOUR_B);
    ZXN_WRITE_REG(0x56, 0);                  /* put MMU6 back */

    ZXN_WRITE_REG(0x70, 0x00);               /* Layer 2: 256x192, 8bpp      */
    ZXN_WRITE_REG(0x12, SCREEN_A_BANK);      /* active bank                 */
    ZXN_WRITE_REG(0x68, 0x80);               /* ULA off, so only L2 shows   */
    IO_123B = 0x02;                          /* Layer 2 visible             */

    build_copper();
    copper_load_and_start();

    for (;;)
        ;
}
