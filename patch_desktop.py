import sys
import re

with open("src/desktop.c", "r") as f:
    code = f.read()

# 1. Update MAX_WINDOWS and WIN_ definitions
code = code.replace("""#define MAX_WINDOWS 6
#define WIN_TERM 0
#define WIN_FILES 1
#define WIN_NOTES 2
#define WIN_ABOUT 3
#define WIN_AI 4""", """#define MAX_WINDOWS 8
#define WIN_TERM 0
#define WIN_FILES 1
#define WIN_NOTES 2
#define WIN_ABOUT 3
#define WIN_AI 4
#define WIN_CALC 5
#define WIN_SYSMON 6""")

# 2. Update window_t struct
code = code.replace("""    char note_file[20];
    int file_sel;
} window_t;""", """    char note_file[20];
    int file_sel;
    int calc_v1;
    int calc_op;
    int calc_new;
    int sysmon_history[60];
    int sysmon_head;
} window_t;""")

# 3. Update Icons
code = code.replace("""static desktop_icon_t icons[MAX_ICONS] = {
    {50, 40,   "AI Chat",  5},
    {50, 150,  "Terminal", 0},
    {50, 260,  "Files",    1},
    {50, 370,  "Notes",    2},
    {50, 480,  "About",    3},
};
static int num_icons = 5;""", """#undef MAX_ICONS
#define MAX_ICONS 7
static desktop_icon_t icons[MAX_ICONS] = {
    {50, 40,   "AI Chat",  WIN_AI},
    {50, 150,  "Terminal", WIN_TERM},
    {50, 260,  "Files",    WIN_FILES},
    {50, 370,  "Notes",    WIN_NOTES},
    {50, 480,  "About",    WIN_ABOUT},
    {50, 590,  "Calc",     WIN_CALC},
    {50, 700,  "SysMon",   WIN_SYSMON},
};
static int num_icons = 7;""")

# 4. Kickoff Menu items
code = code.replace("""#define KO_ITEMS  7
#define KO_ITEM_H 36
#define KO_HEADER  64
#define KO_H      (KO_HEADER + KO_ITEMS * KO_ITEM_H + 8)

static const char *ko_labels[KO_ITEMS] = {
    "AI Chat", "Terminal", "Files", "Notes", "About",
    "--------", "Shut Down"
};
static int ko_ids[KO_ITEMS] = {5,0,1,2,3,-1,-2};""", """#define KO_ITEMS  9
#define KO_ITEM_H 36
#define KO_HEADER  64
#define KO_H      (KO_HEADER + KO_ITEMS * KO_ITEM_H + 8)

static const char *ko_labels[KO_ITEMS] = {
    "AI Chat", "Terminal", "Files", "Notes", "Calc", "SysMonitor", "About",
    "--------", "Shut Down"
};
static int ko_ids[KO_ITEMS] = {WIN_AI,WIN_TERM,WIN_FILES,WIN_NOTES,WIN_CALC,WIN_SYSMON,WIN_ABOUT,-1,-2};""")

# 5. Context menu code
ctx_menu_code = """
/* ── Context Menu ─────────────────────────────────────────── */
static int ctx_menu_open = 0;
static int ctx_menu_x = 0;
static int ctx_menu_y = 0;
#define CTX_W 240
#define CTX_ITEMS 3
#define CTX_ITEM_H 36
#define CTX_H (CTX_ITEMS * CTX_ITEM_H + 8)
static const char *ctx_labels[CTX_ITEMS] = { "New Note", "System Monitor", "Refresh Wallpaper" };

static void draw_context_menu(void) {
    if (!ctx_menu_open) return;
    vga_bb_fill_rounded_rect(ctx_menu_x+4, ctx_menu_y+4, CTX_W, CTX_H, 8, B_SHADOW);
    vga_bb_fill_rounded_rect(ctx_menu_x, ctx_menu_y, CTX_W, CTX_H, 6, B_KICKOFF);
    vga_bb_draw_rect_outline(ctx_menu_x, ctx_menu_y, CTX_W, CTX_H, B_SEPARATOR);
    mouse_state_t ms; mouse_get_state(&ms);
    for (int i=0; i<CTX_ITEMS; i++) {
        int iy = ctx_menu_y + 4 + i*CTX_ITEM_H;
        int hover = (ms.x >= ctx_menu_x+4 && ms.x < ctx_menu_x+CTX_W-4 && ms.y >= iy && ms.y < iy+CTX_ITEM_H);
        if (hover) vga_bb_fill_rounded_rect(ctx_menu_x+4, iy+2, CTX_W-8, CTX_ITEM_H-4, 4, B_KICKOFF_HL);
        vga_bb_draw_string_2x(ctx_menu_x+10, iy+10, ctx_labels[i], B_TEXT, 0x00000000);
    }
}
"""

code = code.replace("""/* ── Kickoff Menu (Centered above launcher) ───────────────── */""", ctx_menu_code + "\n/* ── Kickoff Menu (Centered above launcher) ───────────────── */")

# 6. Icons visual patch
icons_visuals = """        case WIN_AI: /* AI Chat */
            vga_bb_fill_rounded_rect(cx-1, cy+1, 32, 26, 6, B_SHADOW);
            vga_bb_fill_rounded_rect(cx-2, cy, 32, 26, 6, B_ACCENT);
            vga_bb_fill_rounded_rect(cx, cy+2, 28, 22, 5, B_BG);
            vga_bb_draw_string_2x(cx+3, cy+6, "AI", 0x80000000, 0x00000000);
            vga_bb_draw_string_2x(cx+2, cy+5, "AI", B_ACCENT, 0x00000000);
            vga_bb_fill_circle(cx+28, cy+2, 4, B_YELLOW);
            break;
        case WIN_CALC:
            vga_bb_fill_rounded_rect(cx-2, cy, 28, 34, 4, B_BG_DARK);
            vga_bb_draw_rect_outline(cx-2, cy, 28, 34, B_ACCENT2);
            vga_bb_fill_rect(cx, cy+4, 24, 8, B_BG_ALT);
            vga_bb_draw_string_2x(cx+10, cy+1, "=", B_GREEN, 0x00000000);
            for(int yy=0;yy<3;yy++) for(int xx=0;xx<3;xx++) vga_bb_fill_rect(cx+2+xx*8, cy+16+yy*6, 6, 4, B_SEPARATOR);
            break;
        case WIN_SYSMON:
            vga_bb_fill_rounded_rect(cx-2, cy, 32, 28, 4, B_WIN_BG);
            vga_bb_draw_rect_outline(cx-2, cy, 32, 28, B_ACCENT);
            vga_bb_fill_rect(cx+2, cy+16, 6, 10, B_GREEN);
            vga_bb_fill_rect(cx+10, cy+10, 6, 16, B_YELLOW);
            vga_bb_fill_rect(cx+18, cy+6, 6, 20, B_RED);
            break;"""
code = code.replace("""        case 5: /* AI Chat */
            vga_bb_fill_rounded_rect(cx-1, cy+1, 32, 26, 6, B_SHADOW);
            vga_bb_fill_rounded_rect(cx-2, cy, 32, 26, 6, B_ACCENT);
            vga_bb_fill_rounded_rect(cx, cy+2, 28, 22, 5, B_BG);
            vga_bb_draw_string_2x(cx+3, cy+6, "AI", 0x80000000, 0x00000000);
            vga_bb_draw_string_2x(cx+2, cy+5, "AI", B_ACCENT, 0x00000000);
            vga_bb_fill_circle(cx+28, cy+2, 4, B_YELLOW);
            break;""", icons_visuals)

# 7. open_window dispatch
code = code.replace("""        case WIN_ABOUT: strcpy(w->title,"About"); w->x=420;w->y=280;w->w=620;w->h=420; break;
        case WIN_AI: strcpy(w->title,"AI Chat"); w->x=240;w->y=80;w->w=840;w->h=620;
            term_add_line(w,"SwanOS AI Assistant"); term_add_line(w,"Ask me anything!"); break;
    }""", """        case WIN_ABOUT: strcpy(w->title,"About"); w->x=420;w->y=280;w->w=620;w->h=420; break;
        case WIN_AI: strcpy(w->title,"AI Chat"); w->x=240;w->y=80;w->w=840;w->h=620;
            term_add_line(w,"SwanOS AI Assistant"); term_add_line(w,"Ask me anything!"); break;
        case WIN_CALC: strcpy(w->title,"Calculator"); w->x=500;w->y=160;w->w=320;w->h=460; w->input[0]='0'; w->input[1]='\\0'; w->calc_new=1; break;
        case WIN_SYSMON: strcpy(w->title,"System Monitor"); w->x=600;w->y=300;w->w=560;w->h=380; w->sysmon_head=0; memset(w->sysmon_history,0,sizeof(w->sysmon_history)); break;
    }""")

# 8. draw_window extra rendering
w_render = """    else if (w->type == WIN_CALC) {
        vga_bb_fill_rect(cx, cy, cw, ch, B_BG_DARK);
        vga_bb_fill_rounded_rect(cx+10, cy+10, cw-20, 60, 4, B_BG_ALT);
        int tw = (int)strlen(w->input) * CW;
        vga_bb_draw_string_2x(cx+cw-10-tw-8, cy+30, w->input, B_TEXT, 0x00000000);
        const char* btns[20] = { "C", " ", " ", "/", "7", "8", "9", "*", "4", "5", "6", "-", "1", "2", "3", "+", "0", "0", ".", "=" };
        int bw = (cw - 50) / 4; int bh = (ch - 90 - 40) / 5;
        for (int i=0; i<20; i++) {
            if (i == 17) continue; /* merged zero */
            int bx = cx + 10 + (i%4)*(bw+10); int by = cy + 80 + (i/4)*(bh+10);
            int rbw = bw; if (i == 16) rbw = bw*2 + 10;
            uint32_t bg = B_WIN_BG; if (i%4 == 3 || i == 19) bg = B_ACCENT2; else if (i < 3) bg = B_SEPARATOR;
            vga_bb_fill_rounded_rect(bx, by, rbw, bh, 6, bg);
            int sw = (int)strlen(btns[i]) * CW;
            vga_bb_draw_string_2x(bx+(rbw-sw)/2, by+(bh-CH)/2, btns[i], B_TEXT, 0x00000000);
        }
    }
    else if (w->type == WIN_SYSMON) {
        vga_bb_fill_rect(cx, cy, cw, ch, B_BG_DARK);
        vga_bb_draw_string_2x(cx+10, cy+10, "Memory Usage History", B_ACCENT, 0x00000000);
        vga_bb_draw_hline(cx+10, cy+40, cw-20, B_SEPARATOR);
        int g_x = cx+20, g_y = cy+60, g_w = cw-40, g_h = ch-120;
        vga_bb_fill_rect(g_x, g_y, g_w, g_h, B_WIN_BG);
        vga_bb_draw_rect_outline(g_x, g_y, g_w, g_h, B_BORDER);
        int max_m = mem_total() / 1024; if (max_m <= 0) max_m = 131072;
        int bar_w = g_w / 60;
        for (int i=0; i<60; i++) {
            int val = w->sysmon_history[(w->sysmon_head + i) % 60];
            if (val > 0) {
                int bh = (val * g_h) / max_m; if (bh > g_h) bh = g_h;
                uint32_t bc = B_GREEN; if (bh > g_h/2) bc = B_YELLOW; if (bh > (g_h*4)/5) bc = B_RED;
                vga_bb_fill_rect(g_x + i*bar_w + 1, g_y + g_h - bh, bar_w-2, bh, bc);
            }
        }
        char mtext[60]; char tmp[16]; strcpy(mtext, "Used: "); itoa(mem_used()/1024, tmp, 10); strcat(mtext, tmp);
        strcat(mtext, " KB / "); itoa(max_m, tmp, 10); strcat(mtext, tmp); strcat(mtext, " KB");
        vga_bb_draw_string_2x(cx+20, cy+ch-40, mtext, B_TEXT, 0x00000000);
    }
"""
code = code.replace("""        vga_bb_draw_string_2x(cx+12, ay, "User:", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+12+6*CW, ay, user_current(), B_ACCENT, 0x00000000);
    }
}""", """        vga_bb_draw_string_2x(cx+12, ay, "User:", B_TEXT_DIM, 0x00000000);
        vga_bb_draw_string_2x(cx+12+6*CW, ay, user_current(), B_ACCENT, 0x00000000);
    }
""" + w_render + "}")

# 9. handle_click extra detection
h_click = """    if (ctx_menu_open) {
        if (mx >= ctx_menu_x && mx < ctx_menu_x+CTX_W && my >= ctx_menu_y && my < ctx_menu_y+CTX_H) {
            int idx = (my - ctx_menu_y - 4) / CTX_ITEM_H;
            if (idx == 0) open_window(WIN_NOTES);
            else if (idx == 1) open_window(WIN_SYSMON);
            else if (idx == 2) wp_cached = 0;
        }
        ctx_menu_open = 0; return 0;
    }"""
    
code = code.replace("""static int handle_click(int mx, int my) {
    if (kickoff_open) {""", """static int handle_click(int mx, int my) {
""" + h_click + """
    if (kickoff_open) {""")

calc_clicks = """            /* Calc click */
            if (w->type == WIN_CALC) {
                int cx2 = w->x+4, cy2 = w->y+TITLEBAR_H+4, cw2 = w->w-8, ch2 = w->h-TITLEBAR_H-8;
                int bw = (cw2 - 50) / 4, bh = (ch2 - 90 - 40) / 5;
                for (int i=0; i<20; i++) {
                    if (i == 17) continue;
                    int bx = cx2 + 10 + (i%4)*(bw+10), by = cy2 + 80 + (i/4)*(bh+10);
                    int rbw = bw; if (i == 16) rbw = bw*2 + 10;
                    if (mx >= bx && mx < bx+rbw && my >= by && my < by+bh) {
                        const char* btns[20] = { "C", " ", " ", "/", "7", "8", "9", "*", "4", "5", "6", "-", "1", "2", "3", "+", "0", "0", ".", "=" };
                        char btn = btns[i][0];
                        if (btn >= '0' && btn <= '9') {
                            if (w->calc_new || w->input[0]=='0') { w->input[0]=btn; w->input[1]='\\0'; w->calc_new=0; }
                            else { int l = strlen(w->input); if (l < 15) { w->input[l]=btn; w->input[l+1]='\\0'; } }
                        }
                        else if (i == 0) { w->input[0]='0'; w->input[1]='\\0'; w->calc_new=1; w->calc_v1=0; w->calc_op=0; }
                        else if (btn == '+' || btn == '-' || btn == '*' || btn == '/') {
                            int v = 0; int neg=0; char *p=w->input; if(*p=='-'){neg=1;p++;} for(; *p; p++) if(*p>='0'&&*p<='9') v=v*10+(*p-'0'); if(neg)v=-v;
                            if (w->calc_op && !w->calc_new) {
                                if (w->calc_op==1) w->calc_v1 += v; else if (w->calc_op==2) w->calc_v1 -= v;
                                else if (w->calc_op==3) w->calc_v1 *= v; else if (w->calc_op==4 && v!=0) w->calc_v1 /= v;
                                int cv=w->calc_v1; char tbuf[20];
                                if(cv<0){ tbuf[0]='-'; itoa(-cv, tbuf+1, 10); } else itoa(cv, tbuf, 10);
                                strcpy(w->input, tbuf);
                            } else w->calc_v1 = v;
                            if(btn=='+')w->calc_op=1; else if(btn=='-')w->calc_op=2; else if(btn=='*')w->calc_op=3; else w->calc_op=4;
                            w->calc_new=1;
                        }
                        else if (btn == '=') {
                            int v = 0; int neg=0; char *p=w->input; if(*p=='-'){neg=1;p++;} for(; *p; p++) if(*p>='0'&&*p<='9') v=v*10+(*p-'0'); if(neg)v=-v;
                            if (w->calc_op) {
                                if (w->calc_op==1) w->calc_v1 += v; else if (w->calc_op==2) w->calc_v1 -= v;
                                else if (w->calc_op==3) w->calc_v1 *= v; else if (w->calc_op==4 && v!=0) w->calc_v1 /= v;
                                int cv=w->calc_v1; char tbuf[20];
                                if(cv<0){ tbuf[0]='-'; itoa(-cv, tbuf+1, 10); } else itoa(cv, tbuf, 10);
                                strcpy(w->input, tbuf); w->calc_op=0; w->calc_new=1;
                            }
                        }
                        return 0;
                    }
                }
            }"""

code = code.replace("""            /* Files click */
            if (w->type==WIN_FILES) { int cy=w->y+TITLEBAR_H+CH+14; int idx=(my-cy)/(CH+4); if(idx>=0) w->file_sel=idx; }
            return 0;""", """            /* Files click */
            if (w->type==WIN_FILES) { int cy=w->y+TITLEBAR_H+CH+14; int idx=(my-cy)/(CH+4); if(idx>=0) w->file_sel=idx; }
""" + calc_clicks + "\n            return 0;")

code = code.replace("""            if (ic->app_id==5){open_window(WIN_AI);return 0;} if(ic->app_id==4) return -4;""", """            if (ic->app_id==WIN_AI){open_window(WIN_AI);return 0;} if(ic->app_id==4) return -4;""")

code = code.replace("""    draw_panel(); draw_kickoff();
    mouse_state_t ms; mouse_get_state(&ms); draw_cursor(ms.x,ms.y);""", """    draw_panel(); draw_kickoff(); draw_context_menu();
    mouse_state_t ms; mouse_get_state(&ms); draw_cursor(ms.x,ms.y);""")

# 10. Update loop in desktop_run to optimize rendering
loop_replacement = """    uint32_t last_draw=0; int needs_redraw=1;
    serial_write("desktop_run: enter loop\\n");

    int last_blink = 0;
    int last_minute = -1;
    uint32_t sysmon_tick = 0;

    while (1) {
        mouse_state_t ms; mouse_get_state(&ms);
        int l_click = (ms.clicked && (ms.buttons & 1));
        int r_click = (ms.clicked && (ms.buttons & 2));
        if (ms.clicked) mouse_clear_events();
        
        if (r_click) {
            /* Right click -> Context menu */
            if (!ctx_menu_open) {
                ctx_menu_open = 1; ctx_menu_x = ms.x; ctx_menu_y = ms.y;
                if (ctx_menu_x + CTX_W > SCRW) ctx_menu_x = SCRW - CTX_W;
                if (ctx_menu_y + CTX_H > SCRH) ctx_menu_y = SCRH - CTX_H;
                needs_redraw = 1;
            }
        }
        else if (l_click && ctx_menu_open) {
             /* Left click might click context menu items or dismiss it */
            handle_click(ms.x, ms.y);
            needs_redraw = 1;
        }
        else if (l_click) {
            if (!dragging) {
                int lr=handle_click(ms.x,ms.y);
                if (lr==-1) { screen_init(); screen_set_serial_mirror(1); screen_clear();
                    screen_set_color(VGA_DARK_GREY,VGA_BLACK); screen_print("\\n\\n   Shutting down...\\n");
                    screen_delay(500); __asm__ volatile("cli; hlt"); while(1); }
                if (lr==-4) { game_snake(); wp_cached=0; }
            }
            needs_redraw=1;
        }
        if (dragging) {
            if (ms.buttons & 1) { /* Left explicitly held */
                int nx=ms.x-drag_ox, ny=ms.y-drag_oy;
                if(nx<0)nx=0; if(ny<0)ny=0;
                if(nx+windows[drag_win].w>SCRW) nx=SCRW-windows[drag_win].w;
                if(ny+windows[drag_win].h>DESK_H) ny=DESK_H-windows[drag_win].h;
                windows[drag_win].x=nx; windows[drag_win].y=ny; needs_redraw=1;
            } else dragging=0;
        }
        if (ms.moved) { needs_redraw=1; }
"""

code_tail = """        uint32_t ticks=timer_get_ticks();
        /* Sysmon history update (every 1 second roughly = 100 ticks at 100Hz) */
        if (ticks - sysmon_tick > 100) {
            for (int i=0; i<MAX_WINDOWS; i++) {
                if (windows[i].active && windows[i].type == WIN_SYSMON) {
                    windows[i].sysmon_history[windows[i].sysmon_head] = mem_used() / 1024;
                    windows[i].sysmon_head = (windows[i].sysmon_head + 1) % 60;
                    needs_redraw = 1;
                }
            }
            sysmon_tick = ticks;
        }

        /* Cursor blink optimization */
        int cur_blink = (ticks / 30) % 2;
        if (cur_blink != last_blink) { last_blink = cur_blink; needs_redraw = 1; }

        /* Clock update optimization */
        rtc_time_t rtc; rtc_read(&rtc);
        if (rtc.minute != last_minute) { last_minute = rtc.minute; needs_redraw = 1; }

        if (needs_redraw || dragging) { draw_desktop(); needs_redraw=0; last_draw = ticks; }
        __asm__ volatile("hlt");
    }"""
    
def find_and_replace_between(text, start_str, end_str, replacement):
    start = text.find(start_str)
    if start == -1: return text
    end = text.find(end_str, start)
    if end == -1: return text
    return text[:start] + replacement + text[end:]

def remove_between(text, start_str, end_str):
    start = text.find(start_str)
    if start == -1: return text
    end = text.find(end_str, start) + len(end_str)
    return text[:start] + text[end:]

code = remove_between(code, """        if (dragging) {""", """        if (ms.moved) { mouse_clear_events(); needs_redraw=1; }""")
code = code.replace("""        if (ms.moved) { mouse_clear_events(); needs_redraw=1; }""", "")

code = find_and_replace_between(code, """    uint32_t last_draw=0; int needs_redraw=1;
    serial_write("desktop_run: enter loop\\n");""", """        if (keyboard_has_key()) {""", loop_replacement)

code = find_and_replace_between(code, """        uint32_t ticks=timer_get_ticks();\n""", """}""", code_tail)

with open("src/desktop.c", "w") as f:
    f.write(code)
