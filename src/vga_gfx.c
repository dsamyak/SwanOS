/* ============================================================
 * SwanOS — VGA Mode 13h Graphics Driver
 * 320x200, 256 colors, direct VGA register programming.
 * Modern dark-theme palette, scaled font, shape primitives.
 * ============================================================ */

#include "vga_gfx.h"
#include "ports.h"
#include "screen.h"

static uint8_t *const VGA_FB = (uint8_t *)0xA0000;
static uint8_t saved_palette[256 * 3];

/* Font data from VGA plane 2 (256 chars * 32 bytes each) */
#define VGA_FONT_SIZE 8192
static uint8_t saved_font[VGA_FONT_SIZE];

/* ── VGA register ports ───────────────────────────────────── */
#define VGA_MISC_WRITE     0x3C2
#define VGA_SEQ_INDEX      0x3C4
#define VGA_SEQ_DATA       0x3C5
#define VGA_CRTC_INDEX     0x3D4
#define VGA_CRTC_DATA      0x3D5
#define VGA_GC_INDEX       0x3CE
#define VGA_GC_DATA        0x3CF
#define VGA_AC_INDEX       0x3C0
#define VGA_AC_WRITE       0x3C0
#define VGA_IS1_READ       0x3DA
#define VGA_DAC_WRITE_IDX  0x3C8
#define VGA_DAC_DATA       0x3C9
#define VGA_DAC_READ_IDX   0x3C7

/* ── Mode 13h registers ───────────────────────────────────── */
static const uint8_t m13_misc = 0x63;
static const uint8_t m13_seq[]  = {0x03,0x01,0x0F,0x00,0x0E};
static const uint8_t m13_crtc[] = {
    0x5F,0x4F,0x50,0x82,0x54,0x80,0xBF,0x1F,
    0x00,0x41,0x00,0x00,0x00,0x00,0x00,0x00,
    0x9C,0x0E,0x8F,0x28,0x40,0x96,0xB9,0xA3,0xFF};
static const uint8_t m13_gc[]   = {0x00,0x00,0x00,0x00,0x00,0x40,0x05,0x0F,0xFF};
static const uint8_t m13_ac[]   = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x41,0x00,0x0F,0x00,0x00};

/* ── Mode 03h (text) registers ────────────────────────────── */
static const uint8_t m03_misc = 0x67;
static const uint8_t m03_seq[]  = {0x03,0x00,0x03,0x00,0x02};
static const uint8_t m03_crtc[] = {
    0x5F,0x4F,0x50,0x82,0x55,0x81,0xBF,0x1F,
    0x00,0x4F,0x0D,0x0E,0x00,0x00,0x00,0x50,
    0x9C,0x0E,0x8F,0x28,0x1F,0x96,0xB9,0xA3,0xFF};
static const uint8_t m03_gc[]   = {0x00,0x00,0x00,0x00,0x00,0x10,0x0E,0x00,0xFF};
static const uint8_t m03_ac[]   = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x14,0x07,
    0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x0C,0x00,0x0F,0x08,0x00};

static void write_regs(uint8_t misc, const uint8_t *seq, int sn,
                       const uint8_t *crtc, int cn,
                       const uint8_t *gc, int gn,
                       const uint8_t *ac, int an)
{
    outb(VGA_MISC_WRITE, misc);
    for (int i = 0; i < sn; i++) { outb(VGA_SEQ_INDEX, i); outb(VGA_SEQ_DATA, seq[i]); }
    outb(VGA_CRTC_INDEX, 0x03); outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) | 0x80);
    outb(VGA_CRTC_INDEX, 0x11); outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) & ~0x80);
    for (int i = 0; i < cn; i++) { outb(VGA_CRTC_INDEX, i); outb(VGA_CRTC_DATA, crtc[i]); }
    for (int i = 0; i < gn; i++) { outb(VGA_GC_INDEX, i); outb(VGA_GC_DATA, gc[i]); }
    for (int i = 0; i < an; i++) { inb(VGA_IS1_READ); outb(VGA_AC_INDEX, i); outb(VGA_AC_WRITE, ac[i]); }
    inb(VGA_IS1_READ); outb(VGA_AC_INDEX, 0x20);
}

/* ── Font save/restore (VGA plane 2) ──────────────────────── */

static void save_font(void) {
    /* Configure VGA to read from plane 2 (font data) */
    outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x04); /* Map Mask: plane 2 */
    outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x06); /* Seq Memory Mode: sequential */
    outb(VGA_GC_INDEX, 0x04); outb(VGA_GC_DATA, 0x02);   /* Read Map Select: plane 2 */
    outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x00);   /* GC Mode: read mode 0 */
    outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x0C);   /* Misc: A0000, 64K window */

    volatile uint8_t *vga = (volatile uint8_t *)0xA0000;
    for (int i = 0; i < VGA_FONT_SIZE; i++)
        saved_font[i] = vga[i];

    /* Restore text-mode plane access */
    outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x03); /* Map Mask: planes 0,1 */
    outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x02); /* Seq Memory Mode: odd/even */
    outb(VGA_GC_INDEX, 0x04); outb(VGA_GC_DATA, 0x00);   /* Read Map Select: plane 0 */
    outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x10);   /* GC Mode: odd/even */
    outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x0E);   /* Misc: B8000, odd/even */
}

static void restore_font(void) {
    /* Configure VGA to write to plane 2 (font data) */
    outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x04); /* Map Mask: plane 2 */
    outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x06); /* Seq Memory Mode: sequential */
    outb(VGA_GC_INDEX, 0x04); outb(VGA_GC_DATA, 0x02);   /* Read Map Select: plane 2 */
    outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x00);   /* GC Mode: write mode 0 */
    outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x0C);   /* Misc: A0000, 64K window */

    volatile uint8_t *vga = (volatile uint8_t *)0xA0000;
    for (int i = 0; i < VGA_FONT_SIZE; i++)
        vga[i] = saved_font[i];

    /* Restore text-mode plane access settings */
    outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x03); /* Map Mask: planes 0,1 */
    outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x02); /* Seq Memory Mode: odd/even */
    outb(VGA_GC_INDEX, 0x04); outb(VGA_GC_DATA, 0x00);   /* Read Map Select: plane 0 */
    outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x10);   /* GC Mode: odd/even */
    outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x0E);   /* Misc: B8000, odd/even */
}

/* ── Modern Dark Palette ──────────────────────────────────── */
static void setup_modern_palette(void) {
    /* 0: true black */
    vga_set_palette(0, 0, 0, 0);
    /* 1: near-black (background base) */
    vga_set_palette(1, 2, 2, 5);
    /* 2: dark bg */
    vga_set_palette(2, 3, 3, 7);
    /* 3: dark grey */
    vga_set_palette(3, 8, 8, 12);
    /* 4: medium grey */
    vga_set_palette(4, 16, 16, 20);
    /* 5: light grey */
    vga_set_palette(5, 28, 28, 32);
    /* 6: near-white */
    vga_set_palette(6, 50, 50, 55);
    /* 7: pure white */
    vga_set_palette(7, 63, 63, 63);

    /* 10-19: cyan/teal accent gradient (primary brand color) */
    for (int i = 0; i < 10; i++) {
        int r = (i * 8) / 9;
        int g = 20 + (i * 43) / 9;
        int b = 30 + (i * 33) / 9;
        vga_set_palette(10 + i, r, g, b);
    }

    /* 20-29: blue accent gradient */
    for (int i = 0; i < 10; i++) {
        int r = (i * 5) / 9;
        int g = (i * 15) / 9;
        int b = 20 + (i * 43) / 9;
        vga_set_palette(20 + i, r, g, b);
    }

    /* 30-39: green accent gradient */
    for (int i = 0; i < 10; i++) {
        int r = (i * 5) / 9;
        int g = 15 + (i * 48) / 9;
        int b = (i * 8) / 9;
        vga_set_palette(30 + i, r, g, b);
    }

    /* 40-49: warm/gold accent gradient */
    for (int i = 0; i < 10; i++) {
        int r = 25 + (i * 38) / 9;
        int g = 18 + (i * 32) / 9;
        int b = (i * 5) / 9;
        vga_set_palette(40 + i, r, g, b);
    }

    /* 50-59: purple accent gradient */
    for (int i = 0; i < 10; i++) {
        int r = 10 + (i * 30) / 9;
        int g = (i * 8) / 9;
        int b = 20 + (i * 43) / 9;
        vga_set_palette(50 + i, r, g, b);
    }

    /* 60-69: Loading ring segment colors (various brightnesses) */
    for (int i = 0; i < 10; i++) {
        int v = 5 + (i * 58) / 9;
        vga_set_palette(60 + i, (v * 3) / 10, (v * 8) / 10, v);
    }

    /* 70: dim ring segment */
    vga_set_palette(70, 4, 4, 8);
    /* 71: bright ring segment */
    vga_set_palette(71, 10, 55, 63);

    /* 80-89: subtle background gradient */
    for (int i = 0; i < 10; i++) {
        vga_set_palette(80 + i, 1 + i/3, 1 + i/3, 3 + i);
    }
}

/* ── Public API ────────────────────────────────────────────── */

void vga_gfx_init(void) {
    /* Save the BIOS text-mode font before we destroy it */
    save_font();

    write_regs(m13_misc, m13_seq, 5, m13_crtc, 25, m13_gc, 9, m13_ac, 21);
    setup_modern_palette();

    /* Save palette for fades */
    outb(VGA_DAC_READ_IDX, 0);
    for (int i = 0; i < 256 * 3; i++)
        saved_palette[i] = inb(VGA_DAC_DATA);

    vga_clear(0);
}

/* Restore standard VGA text-mode DAC palette (16 colors) */
static void restore_text_palette(void) {
    /* Standard VGA DAC values (6-bit per channel, 0-63) */
    static const uint8_t vga_dac[16][3] = {
        { 0,  0,  0},  /*  0: black        */
        { 0,  0, 42},  /*  1: blue         */
        { 0, 42,  0},  /*  2: green        */
        { 0, 42, 42},  /*  3: cyan         */
        {42,  0,  0},  /*  4: red          */
        {42,  0, 42},  /*  5: magenta      */
        {42, 21,  0},  /*  6: brown        */
        {42, 42, 42},  /*  7: light grey   */
        {21, 21, 21},  /*  8: dark grey    */
        {21, 21, 63},  /*  9: light blue   */
        {21, 63, 21},  /* 10: light green  */
        {21, 63, 63},  /* 11: light cyan   */
        {63, 21, 21},  /* 12: light red    */
        {63, 21, 63},  /* 13: light magenta*/
        {63, 63, 21},  /* 14: yellow       */
        {63, 63, 63},  /* 15: white        */
    };
    for (int i = 0; i < 16; i++) {
        outb(VGA_DAC_WRITE_IDX, i);
        outb(VGA_DAC_DATA, vga_dac[i][0]);
        outb(VGA_DAC_DATA, vga_dac[i][1]);
        outb(VGA_DAC_DATA, vga_dac[i][2]);
    }
}

void vga_gfx_exit(void) {
    write_regs(m03_misc, m03_seq, 5, m03_crtc, 25, m03_gc, 9, m03_ac, 21);

    /* Restore the BIOS font to VGA plane 2 */
    restore_font();

    /* Restore standard text-mode DAC palette (fixes black screen) */
    restore_text_palette();

    /* Clear the text buffer */
    uint16_t *tb = (uint16_t *)0xB8000;
    for (int i = 0; i < 80 * 25; i++) tb[i] = 0x0F20;
}

void vga_putpixel(int x, int y, uint8_t color) {
    if (x >= 0 && x < GFX_W && y >= 0 && y < GFX_H)
        VGA_FB[y * GFX_W + x] = color;
}

void vga_clear(uint8_t color) {
    for (int i = 0; i < GFX_W * GFX_H; i++) VGA_FB[i] = color;
}

void vga_fill_rect(int x, int y, int w, int h, uint8_t color) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            vga_putpixel(i, j, color);
}

void vga_draw_hline(int x, int y, int len, uint8_t color) {
    for (int i = 0; i < len; i++) vga_putpixel(x + i, y, color);
}

void vga_draw_vline(int x, int y, int len, uint8_t color) {
    for (int i = 0; i < len; i++) vga_putpixel(x, y + i, color);
}

void vga_draw_circle(int cx, int cy, int r, uint8_t color) {
    int x = 0, y = r, d = 3 - 2 * r;
    while (x <= y) {
        vga_putpixel(cx+x,cy+y,color); vga_putpixel(cx-x,cy+y,color);
        vga_putpixel(cx+x,cy-y,color); vga_putpixel(cx-x,cy-y,color);
        vga_putpixel(cx+y,cy+x,color); vga_putpixel(cx-y,cy+x,color);
        vga_putpixel(cx+y,cy-x,color); vga_putpixel(cx-y,cy-x,color);
        if (d < 0) d += 4*x+6; else { d += 4*(x-y)+10; y--; }
        x++;
    }
}

void vga_fill_circle(int cx, int cy, int r, uint8_t color) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x*x + y*y <= r*r) vga_putpixel(cx+x, cy+y, color);
}

void vga_set_palette(uint8_t idx, uint8_t r, uint8_t g, uint8_t b) {
    outb(VGA_DAC_WRITE_IDX, idx);
    outb(VGA_DAC_DATA, r & 63);
    outb(VGA_DAC_DATA, g & 63);
    outb(VGA_DAC_DATA, b & 63);
}

void vga_set_palette_range(uint8_t start, int count, const uint8_t *rgb) {
    outb(VGA_DAC_WRITE_IDX, start);
    for (int i = 0; i < count * 3; i++) outb(VGA_DAC_DATA, rgb[i] & 63);
}

void vga_vsync(void) {
    while (inb(VGA_IS1_READ) & 8);
    while (!(inb(VGA_IS1_READ) & 8));
}

void vga_fade_from_black(int speed_ms) {
    for (int step = 0; step <= 32; step++) {
        outb(VGA_DAC_WRITE_IDX, 0);
        for (int i = 0; i < 256 * 3; i++)
            outb(VGA_DAC_DATA, (saved_palette[i] * step) / 32);
        vga_vsync();
        screen_delay(speed_ms);
    }
}

void vga_fade_to_black(int speed_ms) {
    static uint8_t cur[256 * 3];
    outb(VGA_DAC_READ_IDX, 0);
    for (int i = 0; i < 256 * 3; i++) cur[i] = inb(VGA_DAC_DATA);
    for (int step = 32; step >= 0; step--) {
        outb(VGA_DAC_WRITE_IDX, 0);
        for (int i = 0; i < 256 * 3; i++)
            outb(VGA_DAC_DATA, (cur[i] * step) / 32);
        vga_vsync();
        screen_delay(speed_ms);
    }
}

/* ── Clean 8x8 bitmap font ─────────────────────────────── */
static const uint8_t font8x8[][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* space */
    {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00}, /* A */
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, /* B */
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, /* C */
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, /* D */
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x7E,0x00}, /* E */
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x60,0x00}, /* F */
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3E,0x00}, /* G */
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, /* H */
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* I */
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00}, /* J */
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, /* K */
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, /* L */
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, /* M */
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, /* N */
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, /* O */
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, /* P */
    {0x3C,0x66,0x66,0x66,0x6A,0x6C,0x36,0x00}, /* Q */
    {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00}, /* R */
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, /* S */
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, /* T */
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, /* U */
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, /* V */
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, /* W */
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, /* X */
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, /* Y */
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, /* Z */
    {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, /* 0 */
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, /* 1 */
    {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00}, /* 2 */
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, /* 3 */
    {0x0E,0x1E,0x36,0x66,0x7F,0x06,0x06,0x00}, /* 4 */
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, /* 5 */
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, /* 6 */
    {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00}, /* 7 */
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, /* 8 */
    {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, /* 9 */
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, /* - */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, /* . */
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, /* : */
    {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00}, /* ! */
};

static int char_to_idx(char c) {
    if (c == ' ') return 0;
    if (c >= 'A' && c <= 'Z') return 1 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 1 + (c - 'a');
    if (c >= '0' && c <= '9') return 27 + (c - '0');
    if (c == '-') return 37;
    if (c == '.') return 38;
    if (c == ':') return 39;
    if (c == '!') return 40;
    return 0;
}

void vga_draw_char(int x, int y, char c, uint8_t color) {
    int idx = char_to_idx(c);
    const uint8_t *g = font8x8[idx];
    for (int r = 0; r < 8; r++) {
        uint8_t bits = g[r];
        for (int col = 0; col < 8; col++)
            if (bits & (0x80 >> col))
                vga_putpixel(x + col, y + r, color);
    }
}

void vga_draw_string(int x, int y, const char *str, uint8_t color) {
    while (*str) { vga_draw_char(x, y, *str, color); x += 9; str++; }
}

/* Draw a character at 2x scale for large headings */
void vga_draw_char_2x(int x, int y, char c, uint8_t color) {
    int idx = char_to_idx(c);
    const uint8_t *g = font8x8[idx];
    for (int r = 0; r < 8; r++) {
        uint8_t bits = g[r];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                vga_putpixel(x + col*2,   y + r*2,   color);
                vga_putpixel(x + col*2+1, y + r*2,   color);
                vga_putpixel(x + col*2,   y + r*2+1, color);
                vga_putpixel(x + col*2+1, y + r*2+1, color);
            }
        }
    }
}

/* Draw string at 3x scale for hero text */
void vga_draw_char_3x(int x, int y, char c, uint8_t color) {
    int idx = char_to_idx(c);
    const uint8_t *g = font8x8[idx];
    for (int r = 0; r < 8; r++) {
        uint8_t bits = g[r];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                for (int dy = 0; dy < 3; dy++)
                    for (int dx = 0; dx < 3; dx++)
                        vga_putpixel(x + col*3 + dx, y + r*3 + dy, color);
            }
        }
    }
}

void vga_draw_string_2x(int x, int y, const char *str, uint8_t color) {
    while (*str) { vga_draw_char_2x(x, y, *str, color); x += 18; str++; }
}

void vga_draw_string_3x(int x, int y, const char *str, uint8_t color) {
    while (*str) { vga_draw_char_3x(x, y, *str, color); x += 26; str++; }
}

/* ── Thick circle for ring shapes ─────────────────────────── */
void vga_draw_ring(int cx, int cy, int r, int thickness, uint8_t color) {
    for (int y = -(r+thickness); y <= (r+thickness); y++) {
        for (int x = -(r+thickness); x <= (r+thickness); x++) {
            int dist_sq = x*x + y*y;
            int outer_sq = (r+thickness) * (r+thickness);
            int inner_sq = (r > thickness) ? (r-thickness) * (r-thickness) : 0;
            if (dist_sq <= outer_sq && dist_sq >= inner_sq)
                vga_putpixel(cx + x, cy + y, color);
        }
    }
}
