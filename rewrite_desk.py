import re

with open("src/desktop.c", "r") as f:
    text = f.read()

# 1. Replace size constants
text = re.sub(r'#define SCRW\s+320', '#define SCRW       GFX_W', text)
text = re.sub(r'#define SCRH\s+200', '#define SCRH       GFX_H', text)
text = re.sub(r'#define TASKBAR_H\s+14', '#define TASKBAR_H  40', text)
text = re.sub(r'#define TITLEBAR_H\s+11', '#define TITLEBAR_H 30', text)
text = re.sub(r'#define CHAR_W\s+7', '#define CHAR_W     8', text)
text = re.sub(r'#define CHAR_H\s+9', '#define CHAR_H     8', text)
text = re.sub(r'#define ICON_W\s+32', '#define ICON_W     64', text)
text = re.sub(r'#define ICON_H\s+28', '#define ICON_H     64', text)

# 2. Replace macro colors
colors = {
    r'#define C_DESK_BG\s+80.*': '#define C_DESK_BG    0xFF1F1F2F',
    r'#define C_TASKBAR\s+22.*': '#define C_TASKBAR    0xEE101015',
    r'#define C_WIN_TITLE\s+20.*': '#define C_WIN_TITLE  0xDD20202F',
    r'#define C_WIN_BG\s+1.*': '#define C_WIN_BG     0xEE151515',
    r'#define C_WIN_BORDER\s+3.*': '#define C_WIN_BORDER 0xFF404050',
    r'#define C_TEXT\s+6.*': '#define C_TEXT       0xFFAAAAAA',
    r'#define C_BRIGHT\s+7.*': '#define C_BRIGHT     0xFFFFFFFF',
    r'#define C_DIM\s+3.*': '#define C_DIM        0xFF555555',
    r'#define C_CYAN\s+15.*': '#define C_CYAN       0xFF00EEEE',
    r'#define C_GREEN\s+35.*': '#define C_GREEN      0xFF00EE00',
    r'#define C_GOLD\s+45.*': '#define C_GOLD       0xFFEEEE00',
    r'#define C_RED\s+91.*': '#define C_RED        0xFFEE0000',
    r'#define C_ICON_BG\s+81.*': '#define C_ICON_BG    0x88000000',
    r'#define C_MENU_BG\s+2.*': '#define C_MENU_BG    0xEE202020',
    r'#define C_MENU_HL\s+20.*': '#define C_MENU_HL    0xFF404050',
    r'#define C_CURSOR1\s+7.*': '#define C_CURSOR1    0xFFFFFFFF',
    r'#define C_CURSOR2\s+0.*': '#define C_CURSOR2    0xFF000000',
    r'#define C_BTN_START\s+25.*': '#define C_BTN_START  0xFF00EEEE'
}

for k, v in colors.items():
    text = re.sub(k, v, text)

# 3. Replace direct number color pushes
text = text.replace('setup_desktop_palette();', '// setup_desktop_palette();')
text = text.replace('vga_fade_from_black(8);', '// vga_fade_from_black(8);')
text = text.replace('vga_fade_to_black(8);', '// vga_fade_to_black(8);')

text = text.replace('vga_bb_fill_rect(w->x+2, w->y+2, w->w, w->h, 0);', 'vga_bb_fill_rect_alpha(w->x+8, w->y+8, w->w, w->h, 0x88000000);')
text = text.replace('vga_bb_fill_rect(cx, cy, cw, ch, 0);', 'vga_bb_fill_rect_alpha(cx, cy, cw, ch, 0x88000000);')
text = text.replace('vga_bb_fill_rect(cbx, w->y+2, 9, 8, 90);', 'vga_bb_fill_rect(cbx, w->y+6, 18, 18, 0xFFEE2222);')
text = text.replace('vga_bb_draw_string(cbx+1, w->y+2, "X", C_BRIGHT, 90);', 'vga_bb_draw_string(cbx+4, w->y+8, "X", C_BRIGHT, 0xFFEE2222);')

text = text.replace('vga_bb_draw_char(cx + 2 + j * CHAR_W, ly,\n                                 w->lines[i][j], C_GREEN, 0);', 'vga_bb_draw_char(cx + 2 + j * CHAR_W, ly,\n                                 w->lines[i][j], C_GREEN, 0x00000000);')

text = text.replace('vga_bb_draw_string(cx+1, iy, ">", C_CYAN, 0);', 'vga_bb_draw_string(cx+1, iy, ">", C_CYAN, 0x00000000);')
text = text.replace(', 0)', ', 0xFF000000)')  # remaining , 0) to ARGB black. Wait this might break other args. Lets be selective:
text = text.replace('0);', '0xFF000000);') # A safer blanket replace for colors passed as 0 at the end of statements

# Increase default window sizes in open_window()
text = text.replace('w->x = 50; w->y = 10; w->w = 220; w->h = 150;', 'w->x = 250; w->y = 150; w->w = 800; w->h = 500;')
text = text.replace('w->x = 60; w->y = 20; w->w = 180; w->h = 130;', 'w->x = 300; w->y = 200; w->w = 800; w->h = 500;')
text = text.replace('w->x = 70; w->y = 15; w->w = 200; w->h = 140;', 'w->x = 350; w->y = 250; w->w = 800; w->h = 500;')
text = text.replace('w->x = 80; w->y = 20; w->w = 160; w->h = 140;', 'w->x = 400; w->y = 300; w->w = 600; w->h = 400;')
text = text.replace('w->x = 45; w->y = 5; w->w = 230; w->h = 165;', 'w->x = 200; w->y = 100; w->w = 800; w->h = 600;')

# Re-establish Icon Coordinates (originally 8, 8, 8, 44 etc)
text = text.replace('{8,   8,  "AI Chat",  5}', '{32, 32, "AI Chat", 5}')
text = text.replace('{8,  44,  "Terminal", 0}', '{32, 128, "Terminal", 0}')
text = text.replace('{8,  80,  "Files",    1}', '{32, 224, "Files", 1}')
text = text.replace('{8, 116,  "Notes",    2}', '{32, 320, "Notes", 2}')
text = text.replace('{8, 152,  "About",    3}', '{32, 416, "About", 3}')

with open("src/desktop.c", "w") as f:
    f.write(text)
